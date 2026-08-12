local Engine = require("Engine")

local function effectResult(frameCount)
    return {
        inputFrameCount = frameCount,
        outputFrameCount = frameCount,
    }
end

---@param delay? number
---@param decay? number
---@param sampleRate? number
---@return sf.SoundSource.EffectProcessor
local function createEcho(delay, decay, sampleRate)
    delay = delay == nil and 0.3 or delay
    decay = decay == nil and 0.5 or decay
    sampleRate = sampleRate == nil and 44100.0 or sampleRate
    local delayFrames = math.max(1, math.floor(math.max(0.0, delay) * sampleRate))
    local delayBuffer = {}
    local bufferIndex = 0

    return function(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        if frameChannelCount == 0 then
            return effectResult(0)
        end
        local requiredSize = delayFrames * frameChannelCount
        for index = #delayBuffer + 1, requiredSize do
            delayBuffer[index] = 0.0
        end
        for frameIndex = 1, frameCount do
            local inputFrame = inputFrames[frameIndex]
            local outputFrame = outputFrames[frameIndex]
            for channelIndex = 1, frameChannelCount do
                local delayIndex = (bufferIndex * frameChannelCount + channelIndex - 1) % #delayBuffer + 1
                local input = inputFrame == nil and 0.0 or inputFrame[channelIndex]
                local delayed = delayBuffer[delayIndex]
                outputFrame[channelIndex] = input + delayed * decay
                delayBuffer[delayIndex] = input
            end
            bufferIndex = (bufferIndex + 1) % delayFrames
        end
        return effectResult(frameCount)
    end
end

---@param drive? number
---@param threshold? number
---@return sf.SoundSource.EffectProcessor
local function createDistortion(drive, threshold)
    drive = drive == nil and 2.0 or drive
    threshold = threshold == nil and 0.7 or threshold
    local limit = math.max(0.0, threshold)

    return function(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        if frameChannelCount == 0 then
            return effectResult(0)
        end
        for frameIndex = 1, frameCount do
            local inputFrame = inputFrames[frameIndex]
            local outputFrame = outputFrames[frameIndex]
            for channelIndex = 1, frameChannelCount do
                local input = inputFrame == nil and 0.0 or inputFrame[channelIndex]
                outputFrame[channelIndex] = Engine.Clamp(input * drive, -limit, limit)
            end
        end
        return effectResult(frameCount)
    end
end

---@param depth? number
---@param bubbleIntensity? number
---@param sampleRate? number
---@return sf.SoundSource.EffectProcessor
local function createUnderwater(depth, bubbleIntensity, sampleRate)
    depth = depth == nil and 0.7 or depth
    bubbleIntensity = bubbleIntensity == nil and 0.3 or bubbleIntensity
    sampleRate = sampleRate == nil and 44100.0 or sampleRate
    local safeRate = math.max(1.0, sampleRate)
    local cutoffFrequency = math.max(1.0, 800.0 - depth * 600.0)
    local timeStep = 1.0 / safeRate
    local alpha = timeStep / (1.0 / (cutoffFrequency * 2.0 * math.pi) + timeStep)
    local bubbleRate = 0.001 + bubbleIntensity * 0.005
    local compressionRatio = 1.0 + depth * 2.0
    local delays = {}
    local buffers = {}
    local indices = {}
    local previous = {}
    local bubblePhase = 0.0
    local waterPhase = 0.0
    local envelope = 0.0
    local randomState = 104729
    local function nextRandom()
        randomState = randomState * 48271 % 2147483647
        return randomState / 2147483647
    end
    for index, milliseconds in ipairs({ 43, 67, 89, 127, 173 }) do
        local delay = math.max(1, math.floor(safeRate * milliseconds / 1000.0))
        delays[index] = delay
        buffers[index] = {}
        indices[index] = 0
    end

    return function(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        if frameChannelCount == 0 then
            return effectResult(0)
        end
        for channelIndex = #previous + 1, frameChannelCount do
            previous[channelIndex] = 0.0
        end
        for index, delay in ipairs(delays) do
            local buffer = buffers[index]
            local requiredSize = delay * frameChannelCount
            for sampleIndex = #buffer + 1, requiredSize do
                buffer[sampleIndex] = 0.0
            end
        end
        for frameIndex = 1, frameCount do
            local modulation = 1.0 + 0.05 * depth * math.sin(waterPhase)
            waterPhase = (waterPhase + 2.0 * math.pi * 0.5 / safeRate) % (2.0 * math.pi)
            local bubble = 0.0
            if nextRandom() < bubbleRate then
                local frequency = 200.0 + nextRandom() * 800.0
                local amplitude = (nextRandom() * 0.5 + 0.5) * bubbleIntensity * 0.1
                bubble = amplitude * math.sin(bubblePhase * frequency) * math.exp(-bubblePhase * 10.0)
            end
            bubblePhase = (bubblePhase + timeStep) % 1.0
            local inputFrame = inputFrames[frameIndex]
            local outputFrame = outputFrames[frameIndex]
            for channelIndex = 1, frameChannelCount do
                local input = inputFrame == nil and 0.0 or inputFrame[channelIndex]
                local signal = input * modulation
                local absolute = math.abs(signal)
                local smoothing = absolute > envelope and 0.95 or 0.999
                envelope = absolute + (envelope - absolute) * smoothing
                if envelope > 0.3 then
                    local gain = 0.3 + (envelope - 0.3) / compressionRatio
                    signal = signal * gain / envelope
                end
                local filtered = alpha * signal + (1.0 - alpha) * previous[channelIndex]
                previous[channelIndex] = filtered
                local reverb = 0.0
                for index, delay in ipairs(delays) do
                    local buffer = buffers[index]
                    local offset = (indices[index] * frameChannelCount + channelIndex - 1) % #buffer + 1
                    reverb = reverb + buffer[offset] * (0.2 + depth * 0.3)
                    buffer[offset] = filtered + buffer[offset] * (0.7 - depth * 0.2)
                end
                local channelBubble = bubble
                if channelIndex ~= 1 then
                    channelBubble = bubble * (0.7 + nextRandom() * 0.6)
                end
                local output = (filtered + reverb * 0.4 + channelBubble) * (1.0 - depth * 0.4)
                outputFrame[channelIndex] = Engine.Clamp(output, -0.8, 0.8)
            end
            for index, delay in ipairs(delays) do
                indices[index] = (indices[index] + 1) % delay
            end
        end
        return effectResult(frameCount)
    end
end

---@param cutoffFrequency? number
---@param transmission? number
---@param sampleRate? number
---@return sf.SoundSource.EffectProcessor
local function createBehindWall(cutoffFrequency, transmission, sampleRate)
    cutoffFrequency = cutoffFrequency == nil and 900.0 or cutoffFrequency
    transmission = transmission == nil and 0.35 or transmission
    sampleRate = sampleRate == nil and 44100.0 or sampleRate
    local safeRate = math.max(1.0, sampleRate)
    local maximumCutoff = math.max(20.0, safeRate * 0.45)
    local cutoff = Engine.Clamp(cutoffFrequency, 20.0, maximumCutoff)
    local alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / safeRate)
    local gain = Engine.Clamp(transmission, 0.0, 1.0)
    local firstStages = {}
    local secondStages = {}

    return function(inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        local frameCount = math.min(inputFrameCount, outputFrameCount)
        if frameChannelCount == 0 then
            return effectResult(0)
        end
        for channelIndex = #firstStages + 1, frameChannelCount do
            firstStages[channelIndex] = 0.0
            secondStages[channelIndex] = 0.0
        end
        for frameIndex = 1, frameCount do
            local inputFrame = inputFrames[frameIndex]
            local outputFrame = outputFrames[frameIndex]
            for channelIndex = 1, frameChannelCount do
                local input = inputFrame == nil and 0.0 or inputFrame[channelIndex]
                local firstStage = firstStages[channelIndex] + alpha * (input - firstStages[channelIndex])
                local secondStage = secondStages[channelIndex] + alpha * (firstStage - secondStages[channelIndex])
                firstStages[channelIndex] = firstStage
                secondStages[channelIndex] = secondStage
                outputFrame[channelIndex] = Engine.Clamp(secondStage * gain, -1.0, 1.0)
            end
        end
        return effectResult(frameCount)
    end
end

local AudioEffects = {}

AudioEffects.EFFECTS = {
    Echo = createEcho,
    Distortion = createDistortion,
    Underwater = createUnderwater,
    BehindWall = createBehindWall,
}

return AudioEffects
