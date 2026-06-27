function compressContext(provider, model, apiKey, baseUrl, messagesJson) {
    const messages = JSON.parse(messagesJson);
    pyLog("[Agent] compressContext: compressing " + messages.length + " messages (" + messagesJson.length + " chars)");
    const conversationText = messages.map(function (m) {
        return m.role + ": " + m.content;
    }).join("\n\n");

    const summaryPrompt = pyReadAgentResource("agent/Prompts/CompressContext.md");
    const postData = {
        model: model,
        temperature: 0.3,
        messages: [
            {
                role: "system",
                content: summaryPrompt
            },
            { role: "user", content: "Summarize this conversation:\n\n" + conversationText }
        ]
    };

    const responseText = pyFetch(baseUrl, apiKey, JSON.stringify(postData));

    try {
        const resultObj = JSON.parse(responseText);
        if (resultObj.error) {
            pyLog("[Agent] compressContext: API error");
            return "";
        }
        if (resultObj.choices && resultObj.choices.length > 0) {
            const summary = resultObj.choices[0].message.content;
            pyLog("[Agent] compressContext: done, summary length=" + summary.length);
            return summary;
        }
        pyLog("[Agent] compressContext: unexpected response");
        return "";
    } catch (e) {
        pyLog("[Agent] compressContext: parse error " + e);
        return "";
    }
}
