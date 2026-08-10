using System;
using Ludork.Plugins.OfficialBlueprintAI.Agent;

namespace Ludork.Plugins.OfficialBlueprintAI.Providers;

internal sealed class AgentTransportFactory : IAgentTransportFactory
{
    public IAgentTransportSession Create(
        string provider,
        string model,
        string endpoint,
        string organization,
        string project,
        string apiKey,
        string instructions)
    {
        if (string.Equals(provider, "OpenAI", StringComparison.OrdinalIgnoreCase))
        {
            return new OpenAIResponsesSession(
                model,
                endpoint,
                organization,
                project,
                apiKey,
                instructions);
        }

        return new OpenAICompatibleChatSession(
            model,
            endpoint,
            apiKey,
            instructions);
    }
}
