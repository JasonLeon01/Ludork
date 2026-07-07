var MAX_WORKFLOW_ITERATIONS = 30;
var MAX_FORMAT_RETRIES = 5;
var MAX_RESEARCH_ITERATIONS = 8;
var MODIFICATION_MAX_TOKENS = 8192;
var WORKFLOW_LOG_MAX_CHARS = 12000;
var WORKFLOW_TOOL_RESULT_MAX_CHARS = 8000;

var _agentTools = null;

function _getAgentTools() {
    if (_agentTools === null) {
        _agentTools = JSON.parse(pyGetAgentTools());
    }
    return _agentTools;
}

function _reportStatus(key, detail, iteration) {
    pyReportStatus(JSON.stringify({
        key: key,
        detail: detail || "",
        iteration: iteration || 0
    }));
}

function _looksLikePrematureReply(text) {
    if (!text) return false;
    var trimmed = text.trim();
    if (trimmed.length > 180) return false;
    if (/^(我来|让我|我先|接下来|现在让我|正在|准备|需要先|我会先|我将)/.test(trimmed)) {
        return true;
    }
    if (trimmed.length < 100 && /查看|搜索|查找|确认|读取|检查/.test(trimmed) && !/[。！？!?]$/.test(trimmed)) {
        return true;
    }
    return false;
}

function _shouldBlockReply(text) {
    if (!text) return false;
    var trimmed = text.trim();
    if (_looksLikePrematureReply(trimmed)) return true;
    if (/请提供|需要你提供|请告诉我|或者您可以直接|请直接告诉我/.test(trimmed)) return true;
    if (/需要先查看|需要查看|无法读取|无法获取|无法直接/.test(trimmed) && /文件|节点函数|源码|源代码/.test(trimmed)) {
        return true;
    }
    if (/^\d+\.\s+Source\//m.test(trimmed) && /节点函数|文件的内容|文件内容/.test(trimmed)) {
        return true;
    }
    return false;
}

function _truncateForLog(text, maxChars) {
    if (text === undefined || text === null) {
        return "";
    }
    var str = String(text);
    if (str.length <= maxChars) {
        return str;
    }
    return str.substring(0, maxChars) + "\n...[truncated " + (str.length - maxChars) + " chars]";
}

function _logWorkflowMessage(role, content, iteration) {
    if (!content) {
        return;
    }
    var maxChars = WORKFLOW_LOG_MAX_CHARS;
    if (role.indexOf("tool/") === 0) {
        maxChars = WORKFLOW_TOOL_RESULT_MAX_CHARS;
    }
    var prefix = "[Workflow";
    if (iteration) {
        prefix += " iter=" + iteration;
    }
    prefix += "] " + role + ": ";
    pyLog(prefix + _truncateForLog(content, maxChars));
}

function _logAssistantTurn(choiceMessage, iteration) {
    var content = choiceMessage.content || "";
    var reasoning = choiceMessage.reasoning_content || "";
    if (content) {
        _logWorkflowMessage("assistant", content, iteration);
    }
    if (reasoning) {
        _logWorkflowMessage("assistant/reasoning", reasoning, iteration);
    }
    var toolCalls = choiceMessage.tool_calls || [];
    if (toolCalls.length > 0) {
        var parts = [];
        for (var i = 0; i < toolCalls.length; i++) {
            var tc = toolCalls[i];
            var fn = tc.function || {};
            parts.push((fn.name || "unknown") + "(" + _truncateForLog(fn.arguments || "", 300) + ")");
        }
        _logWorkflowMessage("assistant/tool_calls", parts.join("\n"), iteration);
    }
}

function _logApiResponseSummary(resultObj, responseText) {
    if (!resultObj || !resultObj.choices || resultObj.choices.length === 0) {
        pyLog("[Agent] API response (unparsed): " + _truncateForLog(responseText, 2000));
        return;
    }
    var choice = resultObj.choices[0];
    var message = choice.message || {};
    var usage = resultObj.usage || {};
    pyLog(
        "[Agent] API response summary: id=" + (resultObj.id || "") +
        " model=" + (resultObj.model || "") +
        " finish_reason=" + (choice.finish_reason || "") +
        " content_chars=" + String(message.content || "").length +
        " reasoning_chars=" + String(message.reasoning_content || "").length +
        " tool_calls=" + ((message.tool_calls && message.tool_calls.length) || 0) +
        " prompt_tokens=" + (usage.prompt_tokens || 0) +
        " completion_tokens=" + (usage.completion_tokens || 0)
    );
}

function _loadAgentPrompt(relPath) {
    return pyReadAgentResource(relPath);
}

function _buildFormatRetryMessage(expectsModification) {
    var base = _loadAgentPrompt("agent/Prompts/FormatRetryBase.md");
    if (expectsModification) {
        return base + "\n\n" + _loadAgentPrompt("agent/Prompts/FormatRetryModificationSuffix.md");
    }
    return base + "\n\n" + _loadAgentPrompt("agent/Prompts/FormatRetryGeneralSuffix.md");
}

function _buildPrematureReplyMessage(expectsModification) {
    if (expectsModification) {
        return _loadAgentPrompt("agent/Prompts/PrematureReplyModification.md");
    }
    return _loadAgentPrompt("agent/Prompts/PrematureReplyGeneral.md");
}

function _buildResearchLimitMessage() {
    return _loadAgentPrompt("agent/Prompts/ResearchLimitModification.md");
}

function _shouldBlockModificationReply(expectsModification, blueprintModified) {
    return expectsModification && !blueprintModified;
}

function _parseToolArguments(toolCall) {
    var fn = toolCall.function || {};
    var rawArgs = fn.arguments || "";
    if (typeof rawArgs !== "string") {
        rawArgs = JSON.stringify(rawArgs);
    }
    try {
        return { args: JSON.parse(rawArgs), error: null };
    } catch (e) {
        return { args: null, error: "Invalid JSON in tool arguments for " + (fn.name || "unknown") + ": " + e };
    }
}

function _executeToolCall(toolCall, iteration) {
    var fn = toolCall.function || {};
    var toolName = fn.name || "";
    var parsed = _parseToolArguments(toolCall);
    if (parsed.error) {
        return { error: parsed.error, terminate: false, statusKey: "format_retry" };
    }
    var args = parsed.args;

    if (toolName === "search_project") {
        var keywords = args && args.keywords ? String(args.keywords) : "";
        if (!keywords.trim()) {
            return { error: "search_project requires a non-empty keywords argument.", terminate: false, statusKey: "format_retry" };
        }
        _reportStatus("fetchfile", keywords, iteration);
        var fetchResult = pyFetchFile(keywords);
        pyLog("[Agent] -> search_project result length=" + fetchResult.length);
        return { content: fetchResult, terminate: false, statusKey: "fetchfile" };
    }

    if (toolName === "read_file") {
        var path = args && args.path ? String(args.path) : "";
        if (!path.trim()) {
            return { error: "read_file requires a non-empty path argument.", terminate: false, statusKey: "format_retry" };
        }
        var startLine = args && args.start_line != null ? Number(args.start_line) : 0;
        var endLine = args && args.end_line != null ? Number(args.end_line) : 0;
        var symbol = args && args.symbol ? String(args.symbol) : "";
        if (!isFinite(startLine) || startLine < 0) {
            startLine = 0;
        }
        if (!isFinite(endLine) || endLine < 0) {
            endLine = 0;
        }
        _reportStatus("readfile", path, iteration);
        var readResult = pyReadFile(path, startLine, endLine, symbol);
        pyLog("[Agent] -> read_file result length=" + readResult.length);
        return { content: readResult, terminate: false, statusKey: "readfile" };
    }

    if (toolName === "run_terminal") {
        var command = args && args.command ? String(args.command) : "";
        if (!command.trim()) {
            return { error: "run_terminal requires a non-empty command argument.", terminate: false, statusKey: "format_retry" };
        }
        _reportStatus("fileprocess", command, iteration);
        var cmdResult = pyRunTerminal(command, 120);
        pyLog("[Agent] -> run_terminal result=" + cmdResult);
        return { content: cmdResult, terminate: false, statusKey: "fileprocess" };
    }

    if (toolName === "patch_blueprint") {
        var ops = args && args.ops ? args.ops : null;
        if (!ops || !ops.length) {
            return { error: "patch_blueprint requires a non-empty ops array.", terminate: false, statusKey: "format_retry" };
        }
        _reportStatus("patchblueprint", "", iteration);
        var patchResult = pyPatchBlueprint(JSON.stringify(ops));
        pyLog("[Agent] -> patch_blueprint result=" + patchResult);
        if (patchResult.indexOf("Error") === 0) {
            return { content: patchResult, terminate: false, statusKey: "patchblueprint" };
        }
        return { content: patchResult, terminate: true, statusKey: "patchblueprint", blueprintModified: true };
    }

    if (toolName === "replace_blueprint") {
        var blueprint = args && args.blueprint ? args.blueprint : null;
        if (!blueprint || typeof blueprint !== "object") {
            return { error: "replace_blueprint requires a blueprint object.", terminate: false, statusKey: "format_retry" };
        }
        _reportStatus("replacefile", "", iteration);
        var replaceResult = pyReplaceFile(JSON.stringify(blueprint));
        pyLog("[Agent] -> replace_blueprint result=" + replaceResult);
        if (replaceResult.indexOf("Error") === 0) {
            return {
                content: (
                    replaceResult +
                    "\n\nDo NOT include \"isJson\" or \"type\" in blueprint JSON. " +
                    "Omit empty event graphs from nodeGraph — never use \"(empty)\" strings. " +
                    "Retry the full intended blueprint change (not just validation fixes). " +
                    "If this was a small fix, prefer patch_blueprint with targeted ops instead of re-outputting the full blueprint."
                ),
                terminate: false,
                statusKey: "replacefile"
            };
        }
        return { content: replaceResult, terminate: true, statusKey: "replacefile", blueprintModified: true };
    }

    return { error: "Unknown tool \"" + toolName + "\".", terminate: false, statusKey: "format_retry" };
}

function _logToolCallsSummary(toolCalls) {
    var parts = [];
    for (var i = 0; i < toolCalls.length; i++) {
        var tc = toolCalls[i];
        var fn = tc.function || {};
        var argLen = fn.arguments ? String(fn.arguments).length : 0;
        parts.push((fn.name || "unknown") + "(" + argLen + " chars)");
    }
    pyLog("[Agent] tool_calls: [" + parts.join(", ") + "]");
}

function _buildAssistantMessage(choiceMessage) {
    var msg = {
        role: "assistant",
        content: choiceMessage.content !== undefined && choiceMessage.content !== null ? choiceMessage.content : null
    };
    if (choiceMessage.tool_calls && choiceMessage.tool_calls.length > 0) {
        msg.tool_calls = choiceMessage.tool_calls;
    }
    return msg;
}

function runWorkflow(provider, model, apiKey, baseUrl, systemPrompt, fileTree, blueprintDataJson, userInput, contextJson, expectsModificationJson) {
    pyLog("[Agent] runWorkflow start | model=" + model + " | userInput=" + userInput);
    pyLog("[Agent] systemPrompt length=" + (systemPrompt ? systemPrompt.length : 0) + " chars");
    pyLog("[Agent] fileTree length=" + (fileTree ? fileTree.length : 0) + " chars");
    pyLog("[Agent] blueprintData length=" + (blueprintDataJson ? blueprintDataJson.length : 0) + " chars");

    var expectsModification = expectsModificationJson === true || expectsModificationJson === "true";
    pyLog("[Agent] expectsModification=" + expectsModification);

    var contextMessages = [];
    if (contextJson) {
        try {
            contextMessages = JSON.parse(contextJson);
            pyLog("[Agent] contextMessages count=" + contextMessages.length);
        } catch (e) {
            contextMessages = [];
        }
    }

    var systemContent = systemPrompt;
    if (fileTree) {
        systemContent += "\n\n=== Project File Tree ===\n" + fileTree;
    }
    if (blueprintDataJson) {
        systemContent += "\n\n=== Current Blueprint Data (compact JSON) ===\n" + blueprintDataJson;
    }

    var conversationMessages = contextMessages.concat([]);
    conversationMessages.push({ role: "user", content: userInput });

    var iteration = 0;
    var blueprintModified = false;
    var formatRetryCount = 0;
    var researchOnlyCount = 0;

    function _validateBlueprintAfterModify() {
        _reportStatus("validate", "", iteration);
        var validationText = pyValidateBlueprint();
        pyLog("[Agent] -> blueprint validation result=" + validationText);
        try {
            return JSON.parse(validationText);
        } catch (e) {
            return { valid: false, errors: ["Failed to parse blueprint validation result: " + validationText] };
        }
    }

    function _pushBlueprintValidationFailure(validation, prefix) {
        var errorLines = [];
        if (validation.errors && validation.errors.length > 0) {
            errorLines = validation.errors;
        } else {
            errorLines = ["Unknown blueprint validation error"];
        }
        var content = prefix + "\n" + errorLines.join("\n") + "\n\nPlease fix the blueprint and try again.";
        _logWorkflowMessage("user/validation_retry", content, iteration);
        conversationMessages.push({ role: "user", content: content });
    }

    while (true) {
        iteration += 1;
        pyLog("[Agent] === Iteration " + iteration + " ===");

        if (iteration > MAX_WORKFLOW_ITERATIONS) {
            pyLog("[Agent] ERROR: Maximum iterations reached");
            return "Error: Maximum workflow iterations (" + MAX_WORKFLOW_ITERATIONS + ") reached.";
        }

        _reportStatus("calling_api", "", iteration);

        var apiMessages = [{ role: "system", content: systemContent }].concat(conversationMessages);

        var msgSizes = [];
        for (var i = 0; i < apiMessages.length; i++) {
            var m = apiMessages[i];
            var size = m.content ? String(m.content).length : 0;
            if (m.tool_calls) {
                size += JSON.stringify(m.tool_calls).length;
            }
            msgSizes.push(m.role + ":" + size);
        }
        pyLog("[Agent] Sending " + apiMessages.length + " messages, sizes=[" + msgSizes.join(", ") + "]");

        var postData = {
            model: model,
            temperature: 0.2,
            messages: apiMessages,
            tools: _getAgentTools(),
            tool_choice: "auto"
        };
        if (expectsModification || blueprintModified) {
            postData.max_tokens = MODIFICATION_MAX_TOKENS;
        }

        pyLog("[Agent] Calling API...");
        var responseText = pyFetch(baseUrl, apiKey, JSON.stringify(postData));
        var resultObj;
        try {
            resultObj = JSON.parse(responseText);
        } catch (e) {
            pyLog("[Agent] ERROR: Failed to parse API response as JSON");
            pyLog("[Agent] API response raw: " + _truncateForLog(responseText, 2000));
            return "Failed to parse AI response as JSON. Response: " + responseText;
        }
        _logApiResponseSummary(resultObj, responseText);

        if (resultObj.error) {
            pyLog("[Agent] API returned error: " + JSON.stringify(resultObj.error));
            return "API Error: " + JSON.stringify(resultObj.error);
        }

        if (!resultObj.choices || resultObj.choices.length === 0) {
            pyLog("[Agent] ERROR: Unexpected API response structure");
            return "Unexpected API response: " + responseText;
        }

        var choice = resultObj.choices[0];
        var choiceMessage = choice.message || {};
        var aiContent = choiceMessage.content || "";
        var toolCalls = choiceMessage.tool_calls || [];
        var finishReason = choice.finish_reason || "";

        if (toolCalls.length > 0) {
            _logToolCallsSummary(toolCalls);
        }
        _logAssistantTurn(choiceMessage, iteration);

        if (toolCalls.length === 0) {
            if (!aiContent || !String(aiContent).trim()) {
                pyLog("[Agent] Empty AI content with no tool calls, requesting retry");
                var emptyRetryMsg = "ERROR: Empty response. Provide a text answer or call a tool.";
                _logWorkflowMessage("user/format_retry", emptyRetryMsg, iteration);
                _reportStatus("format_retry", "", iteration);
                conversationMessages.push({
                    role: "user",
                    content: emptyRetryMsg
                });
                formatRetryCount += 1;
                if (formatRetryCount > MAX_FORMAT_RETRIES) {
                    return "Error: AI repeatedly returned empty responses.";
                }
                continue;
            }

            if (_shouldBlockReply(aiContent)) {
                pyLog("[Agent] -> blocked premature reply, continuing workflow");
                _reportStatus("continue_retry", "", iteration);
                conversationMessages.push({ role: "assistant", content: aiContent });
                var prematureMsg = _buildPrematureReplyMessage(expectsModification);
                _logWorkflowMessage("user/continue_retry", prematureMsg, iteration);
                conversationMessages.push({ role: "user", content: prematureMsg });
                continue;
            }

            if (_shouldBlockModificationReply(expectsModification, blueprintModified)) {
                pyLog("[Agent] -> blocked reply without blueprint save for modification request");
                _reportStatus("continue_retry", "", iteration);
                conversationMessages.push({ role: "assistant", content: aiContent });
                var modificationMsg = _buildPrematureReplyMessage(true);
                _logWorkflowMessage("user/continue_retry", modificationMsg, iteration);
                conversationMessages.push({ role: "user", content: modificationMsg });
                continue;
            }

            conversationMessages.push({ role: "assistant", content: aiContent });

            if (blueprintModified) {
                pyLog("[Agent] -> reply requested after blueprint modification, validating...");
                var replyValidation = _validateBlueprintAfterModify();
                if (!replyValidation.valid) {
                    pyLog("[Agent] -> blueprint validation failed on reply, continuing workflow");
                    _pushBlueprintValidationFailure(replyValidation, "Blueprint validation failed after modification:");
                    continue;
                }
            }

            pyLog("[Agent] -> final reply, ending workflow");
            _logWorkflowMessage("assistant/final", aiContent, iteration);
            return aiContent;
        }

        formatRetryCount = 0;
        conversationMessages.push(_buildAssistantMessage(choiceMessage));

        var terminateResult = null;
        var terminateContent = choiceMessage.content || "";
        var hadFormatError = false;
        var hadResearchTool = false;
        var hadModifyTool = false;

        for (var t = 0; t < toolCalls.length; t++) {
            var toolCall = toolCalls[t];
            var toolId = toolCall.id || ("call_" + iteration + "_" + t);
            var toolName = (toolCall.function && toolCall.function.name) ? toolCall.function.name : "unknown";
            if (toolName === "search_project" || toolName === "read_file") {
                hadResearchTool = true;
            }
            if (toolName === "patch_blueprint" || toolName === "replace_blueprint") {
                hadModifyTool = true;
            }
            var execResult = _executeToolCall(toolCall, iteration);

            if (execResult.error) {
                pyLog("[Agent] Tool error: " + execResult.error);
                _logWorkflowMessage("tool/" + toolName + "/error", execResult.error, iteration);
                conversationMessages.push({
                    role: "tool",
                    tool_call_id: toolId,
                    content: execResult.error
                });
                hadFormatError = true;
                continue;
            }

            if (execResult.blueprintModified) {
                blueprintModified = true;
            }

            conversationMessages.push({
                role: "tool",
                tool_call_id: toolId,
                content: execResult.content
            });
            _logWorkflowMessage("tool/" + toolName, execResult.content, iteration);

            if (execResult.terminate) {
                terminateResult = execResult.content;
            }
        }

        if (terminateResult !== null) {
            if (terminateContent) {
                return terminateResult + "\n\n" + terminateContent;
            }
            return terminateResult;
        }

        if (hadFormatError) {
            _reportStatus("format_retry", "", iteration);
            var formatRetryMsg = _buildFormatRetryMessage(expectsModification);
            _logWorkflowMessage("user/format_retry", formatRetryMsg, iteration);
            conversationMessages.push({ role: "user", content: formatRetryMsg });
            formatRetryCount += 1;
            if (formatRetryCount > MAX_FORMAT_RETRIES) {
                return "Error: AI repeatedly failed to provide valid tool call arguments.";
            }
        }

        if (expectsModification && !blueprintModified) {
            if (hadResearchTool && !hadModifyTool) {
                researchOnlyCount += 1;
            } else if (hadModifyTool) {
                researchOnlyCount = 0;
            }
            if (researchOnlyCount >= MAX_RESEARCH_ITERATIONS) {
                pyLog("[Agent] -> research-only limit reached, requesting blueprint modification");
                _reportStatus("continue_retry", "", iteration);
                var researchLimitMsg = _buildResearchLimitMessage();
                _logWorkflowMessage("user/research_limit", researchLimitMsg, iteration);
                conversationMessages.push({ role: "user", content: researchLimitMsg });
                researchOnlyCount = 0;
            }
        }

        continue;
    }
}
