local Engine = require("Engine")

local ECHO_DELAY = 0.3
local ECHO_DECAY = 0.5
local DISTORTION_DRIVE = 2.0
local DISTORTION_THRESHOLD = 0.7
local UNDERWATER_DEPTH = 0.7
local UNDERWATER_BUBBLE_INTENSITY = 0.3
local BEHIND_WALL_CUTOFF = 900.0
local BEHIND_WALL_TRANSMISSION = 0.35
local MAXIMUM_CHANNEL_COUNT = 8
local UNDERWATER_RANDOM_MODULUS = 2147483647
local UNDERWATER_RANDOM_MULTIPLIER = 48271
local UNDERWATER_RANDOM_SEED = 104729
local UNDERWATER_DELAY_MILLISECONDS = { 43, 67, 89, 127, 173 }
local UNDERWATER_REFERENCE_SAMPLE_RATE = 48000.0
local UNDERWATER_BUBBLE_DURATION = 0.008
local UNDERWATER_MAXIMUM_BUBBLE_VOICES = 8

local AudioEffects = {}

---@param sampleCount integer
---@return number[]
local function createZeroBuffer(sampleCount)
    local buffer = {}
    for index = 1, sampleCount do
        buffer[index] = 0.0
    end
    return buffer
end

---@param buffer      number[]
---@param sampleCount integer
local function clearSamples(buffer, sampleCount)
    for index = 1, sampleCount do
        buffer[index] = 0.0
    end
end

---@param frameChannelCount integer
local function requireStatefulChannelCount(frameChannelCount)
    assert(
        frameChannelCount > 0 and frameChannelCount <= MAXIMUM_CHANNEL_COUNT,
        "Audio effect channel count must be between 1 and 8"
    )
end

---@param result           { inputFrameCount: integer, outputFrameCount: integer }
---@param inputFrameCount  integer
---@param outputFrameCount integer
---@return { inputFrameCount: integer, outputFrameCount: integer }
local function setFrameResult(result, inputFrameCount, outputFrameCount)
    result.inputFrameCount = inputFrameCount
    result.outputFrameCount = outputFrameCount
    return result
end

---@param control    GlobalCore.AudioEffectControl
---@param sampleRate integer
---@return sf.SoundSource.EffectProcessor
local function createEchoProcessor(control, sampleRate)
    local delayFrames = math.max(1, math.floor(math.max(0.0, ECHO_DELAY) * sampleRate))
    local delayBuffer = createZeroBuffer(delayFrames * MAXIMUM_CHANNEL_COUNT)
    local bufferIndex = 0
    local bufferChannelCount = 0
    local tailFrameCount = 0
    local endOfStreamSeen = false
    local result = { inputFrameCount = 0, outputFrameCount = 0 }

    ---@param frameChannelCount integer
    local function reset(frameChannelCount)
        if bufferChannelCount ~= 0 then
            clearSamples(delayBuffer, #delayBuffer)
        end
        bufferIndex = 0
        bufferChannelCount = frameChannelCount
        tailFrameCount = 0
        endOfStreamSeen = false
        control:finishTail()
    end

    local function processor(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        requireStatefulChannelCount(frameChannelCount)
        if control:isCancelled() then
            if inputFrames == nil then
                return setFrameResult(result, 0, 0)
            end
            local frameCount = math.min(inputFrameCount, outputFrameCount)
            ---@cast frameCount integer
            local sampleCount = frameCount * frameChannelCount
            ---@cast sampleCount integer
            clearSamples(outputFrames, sampleCount)
            return setFrameResult(result, frameCount, frameCount)
        end

        if inputFrames == nil then
            endOfStreamSeen = true
            if bufferChannelCount == 0 or bufferChannelCount ~= frameChannelCount or tailFrameCount == 0 then
                reset(0)
                return setFrameResult(result, 0, 0)
            end

            local frameCount = math.min(tailFrameCount, outputFrameCount)
            ---@cast frameCount integer
            for frame = 0, frameCount - 1 do
                local frameOffset = frame * frameChannelCount
                local delayOffset = bufferIndex * MAXIMUM_CHANNEL_COUNT
                for channel = 1, frameChannelCount do
                    local outputIndex = frameOffset + channel
                    local delayIndex = delayOffset + channel
                    local delayed = delayBuffer[delayIndex]
                    ---@cast delayed number
                    outputFrames[outputIndex] = delayed * ECHO_DECAY
                    delayBuffer[delayIndex] = 0.0
                end
                bufferIndex = (bufferIndex + 1) % delayFrames
            end
            tailFrameCount = tailFrameCount - frameCount
            if control:isCancelled() then
                return setFrameResult(result, 0, 0)
            end
            return setFrameResult(result, 0, frameCount)
        end

        if endOfStreamSeen or bufferChannelCount ~= frameChannelCount then
            reset(frameChannelCount)
        end
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        ---@cast frameCount integer
        local sampleCount = frameCount * frameChannelCount
        ---@cast sampleCount integer
        for frame = 0, frameCount - 1 do
            local frameOffset = frame * frameChannelCount
            local delayOffset = bufferIndex * MAXIMUM_CHANNEL_COUNT
            for channel = 1, frameChannelCount do
                local sampleIndex = frameOffset + channel
                local delayIndex = delayOffset + channel
                local input = inputFrames[sampleIndex]
                local delayed = delayBuffer[delayIndex]
                ---@cast delayed number
                outputFrames[sampleIndex] = input + delayed * ECHO_DECAY
                delayBuffer[delayIndex] = input
            end
            bufferIndex = (bufferIndex + 1) % delayFrames
        end
        if control:isCancelled() then
            clearSamples(outputFrames, sampleCount)
        elseif frameCount > 0 then
            tailFrameCount = delayFrames
            control:beginTail()
        end
        return setFrameResult(result, frameCount, frameCount)
    end

    ---@cast processor sf.SoundSource.EffectProcessor
    return processor
end

---@param control GlobalCore.AudioEffectControl
---@return sf.SoundSource.EffectProcessor
local function createDistortionProcessor(control)
    local result = { inputFrameCount = 0, outputFrameCount = 0 }

    local function processor(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        assert(frameChannelCount > 0, "Audio effect channel count must be positive")
        if inputFrames == nil then
            control:finishTail()
            return setFrameResult(result, 0, 0)
        end
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        ---@cast frameCount integer
        local sampleCount = frameCount * frameChannelCount
        ---@cast sampleCount integer
        if not control:isCancelled() then
            for index = 1, sampleCount do
                outputFrames[index] = Engine.Clamp(
                    inputFrames[index] * DISTORTION_DRIVE, -DISTORTION_THRESHOLD, DISTORTION_THRESHOLD
                )
            end
        end
        if control:isCancelled() then
            clearSamples(outputFrames, sampleCount)
        end
        return setFrameResult(result, frameCount, frameCount)
    end

    ---@cast processor sf.SoundSource.EffectProcessor
    return processor
end

---@param control    GlobalCore.AudioEffectControl
---@param sampleRate integer
---@return sf.SoundSource.EffectProcessor
local function createUnderwaterProcessor(control, sampleRate)
    local timeStep = 1.0 / sampleRate
    local bubbleEventsPerSecond = (0.001 + UNDERWATER_BUBBLE_INTENSITY * 0.005) * UNDERWATER_REFERENCE_SAMPLE_RATE
    local bubbleDurationLastAge = math.max(2, math.floor(UNDERWATER_BUBBLE_DURATION * sampleRate + 0.5))
    local compressionRatio = 1.0 + UNDERWATER_DEPTH * 2.0
    local cutoff = math.max(1.0, 800.0 - UNDERWATER_DEPTH * 600.0)
    local alpha = timeStep / (1.0 / (cutoff * 2.0 * math.pi) + timeStep)
    local delayFrames = {}
    local buffers = {}
    local indices = {}
    local previous = createZeroBuffer(MAXIMUM_CHANNEL_COUNT)
    local bubbleAges = createZeroBuffer(UNDERWATER_MAXIMUM_BUBBLE_VOICES)
    local bubbleLastAges = createZeroBuffer(UNDERWATER_MAXIMUM_BUBBLE_VOICES)
    local bubblePhases = createZeroBuffer(UNDERWATER_MAXIMUM_BUBBLE_VOICES)
    local bubblePhaseSteps = createZeroBuffer(UNDERWATER_MAXIMUM_BUBBLE_VOICES)
    local bubbleAmplitudes = createZeroBuffer(UNDERWATER_MAXIMUM_BUBBLE_VOICES)
    local bubbleChannelGains = {}
    local frameBubbles = createZeroBuffer(MAXIMUM_CHANNEL_COUNT)
    local bufferChannelCount = 0
    local bubbleFramesUntilNext = 0.0
    local waterPhase = 0.0
    local envelope = 0.0
    local randomState = UNDERWATER_RANDOM_SEED
    local result = { inputFrameCount = 0, outputFrameCount = 0 }
    ---@cast delayFrames integer[]
    ---@cast buffers number[][]
    ---@cast indices integer[]
    ---@cast bubbleAges integer[]
    ---@cast bubbleLastAges integer[]
    ---@cast bubbleChannelGains number[][]

    for index, milliseconds in ipairs(UNDERWATER_DELAY_MILLISECONDS) do
        local frameCount = math.max(1, math.floor(sampleRate * milliseconds / 1000.0))
        delayFrames[index] = frameCount
        buffers[index] = createZeroBuffer(frameCount * MAXIMUM_CHANNEL_COUNT)
        indices[index] = 0
    end
    for index = 1, UNDERWATER_MAXIMUM_BUBBLE_VOICES do
        bubbleChannelGains[index] = createZeroBuffer(MAXIMUM_CHANNEL_COUNT)
    end

    ---@return number
    local function nextRandom()
        randomState = randomState * UNDERWATER_RANDOM_MULTIPLIER % UNDERWATER_RANDOM_MODULUS
        return randomState / UNDERWATER_RANDOM_MODULUS
    end

    local function scheduleNextBubble()
        bubbleFramesUntilNext = bubbleFramesUntilNext
            - math.log(1.0 - nextRandom()) * sampleRate / bubbleEventsPerSecond
    end

    local function startBubble()
        local availableVoice = 0
        for index = 1, UNDERWATER_MAXIMUM_BUBBLE_VOICES do
            if bubbleLastAges[index] == 0 and availableVoice == 0 then
                availableVoice = index
            end
        end
        local frequency = 200.0 + nextRandom() * 800.0
        local amplitude = (nextRandom() * 0.5 + 0.5) * UNDERWATER_BUBBLE_INTENSITY * 0.1
        if availableVoice == 0 then
            for _ = 2, MAXIMUM_CHANNEL_COUNT do
                nextRandom()
            end
        else
            local channelGains = bubbleChannelGains[availableVoice]
            ---@cast channelGains number[]
            channelGains[1] = 1.0
            for channel = 2, MAXIMUM_CHANNEL_COUNT do
                channelGains[channel] = 0.7 + nextRandom() * 0.6
            end
            bubbleAges[availableVoice] = 0
            bubbleLastAges[availableVoice] = bubbleDurationLastAge
            bubblePhases[availableVoice] = 0.0
            bubblePhaseSteps[availableVoice] = 2.0 * math.pi * frequency / sampleRate
            bubbleAmplitudes[availableVoice] = amplitude
        end
        scheduleNextBubble()
    end

    ---@param frameChannelCount integer
    local function reset(frameChannelCount)
        if bufferChannelCount ~= 0 then
            for _, buffer in ipairs(buffers) do
                clearSamples(buffer, #buffer)
            end
        end
        clearSamples(previous, #previous)
        clearSamples(bubbleAges, #bubbleAges)
        clearSamples(bubbleLastAges, #bubbleLastAges)
        clearSamples(bubblePhases, #bubblePhases)
        clearSamples(bubblePhaseSteps, #bubblePhaseSteps)
        clearSamples(bubbleAmplitudes, #bubbleAmplitudes)
        clearSamples(frameBubbles, #frameBubbles)
        for _, channelGains in ipairs(bubbleChannelGains) do
            clearSamples(channelGains, #channelGains)
        end
        for index = 1, #indices do
            indices[index] = 0
        end
        bufferChannelCount = frameChannelCount
        bubbleFramesUntilNext = 0.0
        waterPhase = 0.0
        envelope = 0.0
        randomState = UNDERWATER_RANDOM_SEED
        if frameChannelCount ~= 0 then
            scheduleNextBubble()
        end
    end

    local function processor(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        requireStatefulChannelCount(frameChannelCount)
        if inputFrames == nil then
            if bufferChannelCount ~= 0 then
                reset(0)
            end
            control:finishTail()
            return setFrameResult(result, 0, 0)
        end
        if bufferChannelCount ~= frameChannelCount then
            reset(frameChannelCount)
        end
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        ---@cast frameCount integer
        local sampleCount = frameCount * frameChannelCount
        ---@cast sampleCount integer
        if control:isCancelled() then
            clearSamples(outputFrames, sampleCount)
            return setFrameResult(result, frameCount, frameCount)
        end

        for frame = 0, frameCount - 1 do
            local modulation = 1.0 + 0.05 * UNDERWATER_DEPTH * math.sin(waterPhase)
            waterPhase = math.fmod(waterPhase + 2.0 * math.pi * 0.5 / sampleRate, 2.0 * math.pi)
            bubbleFramesUntilNext = bubbleFramesUntilNext - 1.0
            if bubbleFramesUntilNext <= 0.0 then
                startBubble()
            end
            clearSamples(frameBubbles, frameChannelCount)
            for voice = 1, UNDERWATER_MAXIMUM_BUBBLE_VOICES do
                local lastAge = bubbleLastAges[voice]
                ---@cast lastAge integer
                if lastAge ~= 0 then
                    local age = bubbleAges[voice]
                    local amplitude = bubbleAmplitudes[voice]
                    local phase = bubblePhases[voice]
                    local phaseStep = bubblePhaseSteps[voice]
                    local channelGains = bubbleChannelGains[voice]
                    ---@cast age integer
                    ---@cast amplitude number
                    ---@cast phase number
                    ---@cast phaseStep number
                    ---@cast channelGains number[]
                    local window = 0.5 - 0.5 * math.cos(2.0 * math.pi * age / lastAge)
                    local sample = amplitude * math.sin(phase) * window
                    for channel = 1, frameChannelCount do
                        local frameBubble = frameBubbles[channel]
                        local channelGain = channelGains[channel]
                        ---@cast frameBubble number
                        ---@cast channelGain number
                        frameBubbles[channel] = frameBubble + sample * channelGain
                    end
                    bubblePhases[voice] = phase + phaseStep
                    age = age + 1
                    bubbleAges[voice] = age
                    if age > lastAge then
                        bubbleLastAges[voice] = 0
                    end
                end
            end
            local frameOffset = frame * frameChannelCount
            for channel = 1, frameChannelCount do
                local sampleIndex = frameOffset + channel
                local signal = inputFrames[sampleIndex] * modulation
                local absolute = math.abs(signal)
                local smoothing = absolute > envelope and 0.95 or 0.999
                envelope = absolute + (envelope - absolute) * smoothing
                if envelope > 0.3 then
                    local gain = 0.3 + (envelope - 0.3) / compressionRatio
                    signal = signal * gain / envelope
                end
                local previousSample = previous[channel]
                ---@cast previousSample number
                local filtered = alpha * signal + (1.0 - alpha) * previousSample
                previous[channel] = filtered
                local reverb = 0.0
                for index, buffer in ipairs(buffers) do
                    local delayIndex = indices[index]
                    ---@cast delayIndex integer
                    local offset = delayIndex * MAXIMUM_CHANNEL_COUNT + channel
                    local delayed = buffer[offset]
                    ---@cast delayed number
                    reverb = reverb + delayed * (0.2 + UNDERWATER_DEPTH * 0.3)
                    buffer[offset] = filtered + delayed * (0.7 - UNDERWATER_DEPTH * 0.2)
                end
                local frameBubble = frameBubbles[channel]
                ---@cast frameBubble number
                local output = (filtered + reverb * 0.4 + frameBubble) * (1.0 - UNDERWATER_DEPTH * 0.4)
                outputFrames[sampleIndex] = Engine.Clamp(output, -0.8, 0.8)
            end
            for index = 1, #indices do
                local delayIndex = indices[index]
                local delayFrameCount = delayFrames[index]
                ---@cast delayIndex integer
                ---@cast delayFrameCount integer
                local nextDelayIndex = (delayIndex + 1) % delayFrameCount
                ---@cast nextDelayIndex integer
                indices[index] = nextDelayIndex
            end
        end
        if control:isCancelled() then
            clearSamples(outputFrames, sampleCount)
        end
        return setFrameResult(result, frameCount, frameCount)
    end

    ---@cast processor sf.SoundSource.EffectProcessor
    return processor
end

---@param control    GlobalCore.AudioEffectControl
---@param sampleRate integer
---@return sf.SoundSource.EffectProcessor
local function createBehindWallProcessor(control, sampleRate)
    local maximumCutoff = math.max(20.0, sampleRate * 0.45)
    local cutoff = Engine.Clamp(BEHIND_WALL_CUTOFF, 20.0, maximumCutoff)
    local alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / sampleRate)
    local gain = Engine.Clamp(BEHIND_WALL_TRANSMISSION, 0.0, 1.0)
    local firstStages = createZeroBuffer(MAXIMUM_CHANNEL_COUNT)
    local secondStages = createZeroBuffer(MAXIMUM_CHANNEL_COUNT)
    local filterChannelCount = 0
    local result = { inputFrameCount = 0, outputFrameCount = 0 }

    ---@param frameChannelCount integer
    local function reset(frameChannelCount)
        clearSamples(firstStages, #firstStages)
        clearSamples(secondStages, #secondStages)
        filterChannelCount = frameChannelCount
    end

    local function processor(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        requireStatefulChannelCount(frameChannelCount)
        if inputFrames == nil then
            if filterChannelCount ~= 0 then
                reset(0)
            end
            control:finishTail()
            return setFrameResult(result, 0, 0)
        end
        if filterChannelCount ~= frameChannelCount then
            reset(frameChannelCount)
        end
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        ---@cast frameCount integer
        local sampleCount = frameCount * frameChannelCount
        ---@cast sampleCount integer
        if control:isCancelled() then
            clearSamples(outputFrames, sampleCount)
            return setFrameResult(result, frameCount, frameCount)
        end

        for frame = 0, frameCount - 1 do
            local frameOffset = frame * frameChannelCount
            for channel = 1, frameChannelCount do
                local sampleIndex = frameOffset + channel
                local input = inputFrames[sampleIndex]
                local firstStage = firstStages[channel]
                local secondStage = secondStages[channel]
                ---@cast firstStage number
                ---@cast secondStage number
                local first = firstStage + alpha * (input - firstStage)
                local second = secondStage + alpha * (first - secondStage)
                firstStages[channel] = first
                secondStages[channel] = second
                outputFrames[sampleIndex] = Engine.Clamp(second * gain, -1.0, 1.0)
            end
        end
        if control:isCancelled() then
            clearSamples(outputFrames, sampleCount)
        end
        return setFrameResult(result, frameCount, frameCount)
    end

    ---@cast processor sf.SoundSource.EffectProcessor
    return processor
end

local processorFactories = {
    Echo = createEchoProcessor,
    Distortion = createDistortionProcessor,
    Underwater = createUnderwaterProcessor,
    BehindWall = createBehindWallProcessor
}

function AudioEffects.Get(name)
    if name == "nil" then
        return nil
    end
    local factory = processorFactories[name]
    assert(factory ~= nil, "Unknown audio effect: " .. tostring(name))
    return function (source, control, sampleRate)
        assert(sampleRate > 0, "Audio playback device sample rate must be positive")
        if source == nil then
            return factory(control, sampleRate)
        end
        control:attachLuaProcessor(source, name, sampleRate)
    end
end

return AudioEffects
