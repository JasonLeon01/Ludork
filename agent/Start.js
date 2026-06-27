var MAX_WORKFLOW_ITERATIONS = 30;
var MAX_FORMAT_RETRIES = 5;
var MODIFICATION_MAX_TOKENS = 8192;

function _extractJson(text) {
    var start = text.indexOf("{");
    if (start < 0) return null;
    var end = text.lastIndexOf("}");
    if (end <= start) return null;
    var candidate = text.substring(start, end + 1);
    try {
        return JSON.parse(candidate);
    } catch (e) {
        return null;
    }
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

function _assistantHistoryContent(workflowResult) {
    var wfType = workflowResult.type || "reply";
    if (wfType === "replacefile" || wfType === "patchblueprint") {
        return JSON.stringify({
            message: workflowResult.message || "",
            type: wfType
        });
    }
    return JSON.stringify(workflowResult);
}

function _terminalToString(terminal) {
    if (terminal === null || terminal === undefined) {
        return "";
    }
    if (typeof terminal === "string") {
        return terminal;
    }
    return JSON.stringify(terminal);
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

function _shouldBlockModificationReply(expectsModification, blueprintModified) {
    return expectsModification && !blueprintModified;
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
            msgSizes.push(apiMessages[i].role + ":" + apiMessages[i].content.length);
        }
        pyLog("[Agent] Sending " + apiMessages.length + " messages, sizes=[" + msgSizes.join(", ") + "]");

        var postData = {
            model: model,
            temperature: 0.2,
            messages: apiMessages
        };
        if (expectsModification || blueprintModified) {
            postData.max_tokens = MODIFICATION_MAX_TOKENS;
        }

        pyLog("[Agent] Calling API...");
        var responseText = pyFetch(baseUrl, apiKey, JSON.stringify(postData));
        pyLog("[Agent] API response: " + responseText);

        var resultObj;
        try {
            resultObj = JSON.parse(responseText);
        } catch (e) {
            pyLog("[Agent] ERROR: Failed to parse API response as JSON");
            return "Failed to parse AI response as JSON. Response: " + responseText;
        }

        if (resultObj.error) {
            pyLog("[Agent] API returned error: " + JSON.stringify(resultObj.error));
            return "API Error: " + JSON.stringify(resultObj.error);
        }

        var aiContent = "";
        if (resultObj.choices && resultObj.choices.length > 0) {
            var choiceMessage = resultObj.choices[0].message || {};
            aiContent = choiceMessage.content || "";
        } else {
            pyLog("[Agent] ERROR: Unexpected API response structure");
            return "Unexpected API response: " + responseText;
        }

        pyLog("[Agent] AI raw content: " + aiContent);

        if (!aiContent || !String(aiContent).trim()) {
            pyLog("[Agent] Empty AI content, requesting retry");
            _reportStatus("format_retry", "", iteration);
            conversationMessages.push({
                role: "user",
                content: "ERROR: Empty response. Respond with valid JSON only."
            });
            formatRetryCount += 1;
            if (formatRetryCount > MAX_FORMAT_RETRIES) {
                return "Error: AI repeatedly returned empty responses.";
            }
            continue;
        }

        var workflowResult = null;
        var parseMode = "direct";
        try {
            workflowResult = JSON.parse(aiContent);
        } catch (e) {
            pyLog("[Agent] Direct JSON parse failed, trying _extractJson...");
            var extracted = _extractJson(aiContent);
            if (extracted) {
                pyLog("[Agent] Extracted JSON from mixed text");
                workflowResult = extracted;
                parseMode = "extracted";
            }
        }

        if (!workflowResult || typeof workflowResult !== "object") {
            pyLog("[Agent] AI response is not JSON, parseMode=failed");
            _reportStatus("format_retry", "", iteration);
            conversationMessages.push({ role: "assistant", content: aiContent });
            conversationMessages.push({ role: "user", content: _buildFormatRetryMessage(expectsModification) });
            formatRetryCount += 1;
            if (formatRetryCount > MAX_FORMAT_RETRIES) {
                return "Error: AI repeatedly failed to respond in required JSON format. Last response: " + aiContent;
            }
            continue;
        }

        formatRetryCount = 0;

        var msg = workflowResult.message || "";
        var wfType = workflowResult.type || "reply";
        var terminal = workflowResult.terminal;

        if (!msg && parseMode === "extracted") {
            msg = aiContent;
        }

        pyLog("[Agent] Workflow result | type=" + wfType + " | msg=" + msg + " | terminal=" + _terminalToString(terminal));

        if (wfType === "reply" && _shouldBlockReply(msg)) {
            pyLog("[Agent] -> blocked premature reply, continuing workflow");
            _reportStatus("continue_retry", "", iteration);
            conversationMessages.push({ role: "assistant", content: _assistantHistoryContent(workflowResult) });
            conversationMessages.push({ role: "user", content: _buildPrematureReplyMessage(expectsModification) });
            continue;
        }

        if (wfType === "reply" && _shouldBlockModificationReply(expectsModification, blueprintModified)) {
            pyLog("[Agent] -> blocked reply without blueprint save for modification request");
            _reportStatus("continue_retry", "", iteration);
            conversationMessages.push({ role: "assistant", content: _assistantHistoryContent(workflowResult) });
            conversationMessages.push({ role: "user", content: _buildPrematureReplyMessage(true) });
            continue;
        }

        conversationMessages.push({ role: "assistant", content: _assistantHistoryContent(workflowResult) });

        if (wfType === "reply") {
            if (blueprintModified) {
                pyLog("[Agent] -> reply requested after blueprint modification, validating...");
                var replyValidation = _validateBlueprintAfterModify();
                if (!replyValidation.valid) {
                    pyLog("[Agent] -> blueprint validation failed on reply, continuing workflow");
                    _pushBlueprintValidationFailure(replyValidation, "Blueprint validation failed after modification:");
                    continue;
                }
            }
            pyLog("[Agent] -> reply, ending workflow");
            return msg;
        }

        if (wfType === "fileprocess") {
            pyLog("[Agent] -> fileprocess, running: " + terminal);
            _reportStatus("fileprocess", terminal, iteration);
            var cmdResult = pyRunTerminal(terminal, 120);
            pyLog("[Agent] -> terminal result=" + cmdResult);
            conversationMessages.push({ role: "user", content: "Terminal result:\n" + cmdResult });
            continue;
        }

        if (wfType === "fetchfile") {
            pyLog("[Agent] -> fetchfile, searching: " + terminal);
            _reportStatus("fetchfile", terminal, iteration);
            var fetchResult = pyFetchFile(terminal);
            pyLog("[Agent] -> fetch result=" + fetchResult.substring(0, 500));
            conversationMessages.push({ role: "user", content: "File search result for '" + terminal + "':\n" + fetchResult });
            continue;
        }

        if (wfType === "readfile") {
            pyLog("[Agent] -> readfile, reading: " + terminal);
            _reportStatus("readfile", terminal, iteration);
            var readResult = pyReadFile(terminal);
            pyLog("[Agent] -> read result length=" + readResult.length);
            conversationMessages.push({ role: "user", content: "File content for '" + terminal + "':\n" + readResult });
            continue;
        }

        if (wfType === "patchblueprint") {
            pyLog("[Agent] -> patchblueprint, applying ops");
            _reportStatus("patchblueprint", "", iteration);
            var patchResult = pyPatchBlueprint(_terminalToString(terminal));
            pyLog("[Agent] -> patch result=" + patchResult);
            if (patchResult.indexOf("Error") === 0) {
                conversationMessages.push({ role: "user", content: "Blueprint patch failed:\n" + patchResult });
                continue;
            }
            blueprintModified = true;
            return patchResult + "\n\n" + msg;
        }

        if (wfType === "replacefile") {
            pyLog("[Agent] -> replacefile, saving blueprint data");
            _reportStatus("replacefile", "", iteration);
            var replaceResult = pyReplaceFile(_terminalToString(terminal));
            pyLog("[Agent] -> replace result=" + replaceResult);
            if (replaceResult.indexOf("Error") === 0) {
                conversationMessages.push({
                    role: "user",
                    content: (
                        "Blueprint replacement failed:\n" + replaceResult +
                        "\n\nDo NOT include \"isJson\" or \"type\" in blueprint JSON. " +
                        "Omit empty event graphs from nodeGraph — never use \"(empty)\" strings. " +
                        "Retry the full intended blueprint change (not just validation fixes). " +
                        "If this was a small fix, prefer type \"patchblueprint\" with targeted ops instead of re-outputting the full blueprint."
                    )
                });
                continue;
            }
            blueprintModified = true;
            return replaceResult + "\n\n" + msg;
        }

        pyLog("[Agent] -> unknown type, requesting retry");
        _reportStatus("format_retry", "", iteration);
        conversationMessages.push({
            role: "user",
            content: "Unknown workflow type \"" + wfType + "\". Use reply, readfile, fileprocess, fetchfile, patchblueprint, or replacefile."
        });
        continue;
    }
}
