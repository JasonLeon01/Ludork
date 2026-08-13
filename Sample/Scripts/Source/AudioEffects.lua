local Engine = require("Engine")

---@return number
local function getPlaybackSampleRate()
    local sampleRate = assert(
        sf.PlaybackDevice.getDeviceSampleRate(), "Audio playback device sample rate is unavailable"
    )
    assert(sampleRate > 0, "Audio playback device sample rate must be positive")
    return sampleRate
end

local function clearFrameCounts(inputFrameCount, outputFrameCount)
    inputFrameCount.value = 0
    outputFrameCount.value = 0
end

---@param delay? number
---@param decay? number
---@return sf.SoundSource.EffectProcessor
local function createEcho(delay, decay)
    delay = delay == nil and 0.3 or delay
    decay = decay == nil and 0.5 or decay
    ---@cast delay number
    ---@cast decay number
    ---@type number?
    local delayFrames = nil
    local delayBuffer = {}
    local bufferIndex = 0
    local bufferChannelCount = 0
    local tailFrameCount = 0

    local function resetDelayBuffer(frameChannelCount)
        if delayFrames == nil then
            local sampleRate = getPlaybackSampleRate()
            delayFrames = math.max(1, math.floor(math.max(0.0, delay) * sampleRate))
        end
        ---@cast delayFrames integer
        delayBuffer = {}
        for index = 1, delayFrames * frameChannelCount do
            delayBuffer[index] = 0.0
        end
        bufferIndex = 0
        bufferChannelCount = frameChannelCount
        tailFrameCount = 0
    end

    return function (inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        if frameChannelCount == 0 then
            clearFrameCounts(inputFrameCount, outputFrameCount)
            return
        end
        if inputFrames == nil then
            if bufferChannelCount == 0 or bufferChannelCount ~= frameChannelCount then
                clearFrameCounts(inputFrameCount, outputFrameCount)
                return
            end
            ---@cast delayFrames integer
            local frameCount = math.min(tailFrameCount, outputFrameCount.capacity)
            for frameIndex = 1, frameCount do
                local outputOffset = (frameIndex - 1) * frameChannelCount
                for channelIndex = 1, frameChannelCount do
                    local delayIndex = bufferIndex * frameChannelCount + channelIndex
                    outputFrames[outputOffset + channelIndex] = delayBuffer[delayIndex] * decay
                    delayBuffer[delayIndex] = 0.0
                end
                bufferIndex = (bufferIndex + 1) % delayFrames
            end
            inputFrameCount.value = 0
            outputFrameCount.value = frameCount
            tailFrameCount = tailFrameCount - frameCount
            return
        end
        if bufferChannelCount ~= frameChannelCount then
            resetDelayBuffer(frameChannelCount)
        end
        ---@cast delayFrames integer
        local frameCount = math.min(inputFrameCount.value, outputFrameCount.capacity)
        for frameIndex = 1, frameCount do
            local frameOffset = (frameIndex - 1) * frameChannelCount
            for channelIndex = 1, frameChannelCount do
                local sampleIndex = frameOffset + channelIndex
                local delayIndex = bufferIndex * frameChannelCount + channelIndex
                local input = inputFrames[sampleIndex]
                local delayed = delayBuffer[delayIndex]
                ---@cast delayed number
                outputFrames[sampleIndex] = input + delayed * decay
                delayBuffer[delayIndex] = input
            end
            bufferIndex = (bufferIndex + 1) % delayFrames
        end
        inputFrameCount.value = frameCount
        outputFrameCount.value = frameCount
        if frameCount > 0 then
            tailFrameCount = delayFrames
        end
    end
end

---@param drive?     number
---@param threshold? number
---@return sf.SoundSource.EffectProcessor
local function createDistortion(drive, threshold)
    drive = drive == nil and 2.0 or drive
    threshold = threshold == nil and 0.7 or threshold
    ---@cast drive number
    ---@cast threshold number
    local limit = math.max(0.0, threshold)

    return function (inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        if inputFrames == nil or frameChannelCount == 0 then
            clearFrameCounts(inputFrameCount, outputFrameCount)
            return
        end
        local frameCount = math.min(inputFrameCount.value, outputFrameCount.capacity)
        local sampleCount = frameCount * frameChannelCount
        for sampleIndex = 1, sampleCount do
            outputFrames[sampleIndex] = Engine.Clamp(inputFrames[sampleIndex] * drive, -limit, limit)
        end
        inputFrameCount.value = frameCount
        outputFrameCount.value = frameCount
    end
end

---@param depth?           number
---@param bubbleIntensity? number
---@return sf.SoundSource.EffectProcessor
local function createUnderwater(depth, bubbleIntensity)
    depth = depth == nil and 0.7 or depth
    bubbleIntensity = bubbleIntensity == nil and 0.3 or bubbleIntensity
    ---@cast depth number
    ---@cast bubbleIntensity number
    ---@type number?
    local sampleRate = nil
    ---@type number?
    local timeStep = nil
    ---@type number?
    local alpha = nil
    local bubbleRate = 0.001 + bubbleIntensity * 0.005
    local compressionRatio = 1.0 + depth * 2.0
    local delays = {}
    local buffers = {}
    local indices = {}
    local previous = {}
    local bufferChannelCount = 0
    local bubblePhase = 0.0
    local waterPhase = 0.0
    local envelope = 0.0
    local randomState = 104729

    local function nextRandom()
        randomState = randomState * 48271 % 2147483647
        return randomState / 2147483647
    end

    local function initializeProcessor()
        sampleRate = getPlaybackSampleRate()
        local cutoffFrequency = math.max(1.0, 800.0 - depth * 600.0)
        timeStep = 1.0 / sampleRate
        alpha = timeStep / (1.0 / (cutoffFrequency * 2.0 * math.pi) + timeStep)
        for index, milliseconds in ipairs({ 43, 67, 89, 127, 173 }) do
            delays[index] = math.max(1, math.floor(sampleRate * milliseconds / 1000.0))
        end
    end

    local function resetChannelState(frameChannelCount)
        buffers = {}
        indices = {}
        previous = {}
        for channelIndex = 1, frameChannelCount do
            previous[channelIndex] = 0.0
        end
        for index, delayFrames in ipairs(delays) do
            local buffer = {}
            for sampleIndex = 1, delayFrames * frameChannelCount do
                buffer[sampleIndex] = 0.0
            end
            buffers[index] = buffer
            indices[index] = 0
        end
        bufferChannelCount = frameChannelCount
        envelope = 0.0
    end

    return function (inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        if inputFrames == nil or frameChannelCount == 0 then
            clearFrameCounts(inputFrameCount, outputFrameCount)
            return
        end
        if sampleRate == nil then
            initializeProcessor()
        end
        ---@cast sampleRate number
        ---@cast timeStep number
        ---@cast alpha number
        if bufferChannelCount ~= frameChannelCount then
            resetChannelState(frameChannelCount)
        end
        local frameCount = math.min(inputFrameCount.value, outputFrameCount.capacity)
        for frameIndex = 1, frameCount do
            local modulation = 1.0 + 0.05 * depth * math.sin(waterPhase)
            waterPhase = (waterPhase + 2.0 * math.pi * 0.5 / sampleRate) % (2.0 * math.pi)
            local bubble = 0.0
            if nextRandom() < bubbleRate then
                local frequency = 200.0 + nextRandom() * 800.0
                local amplitude = (nextRandom() * 0.5 + 0.5) * bubbleIntensity * 0.1
                bubble = amplitude * math.sin(bubblePhase * frequency) * math.exp(-bubblePhase * 10.0)
            end
            bubblePhase = (bubblePhase + timeStep) % 1.0
            local frameOffset = (frameIndex - 1) * frameChannelCount
            for channelIndex = 1, frameChannelCount do
                local sampleIndex = frameOffset + channelIndex
                local signal = inputFrames[sampleIndex] * modulation
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
                for index in ipairs(delays) do
                    local buffer = buffers[index]
                    local offset = indices[index] * frameChannelCount + channelIndex
                    reverb = reverb + buffer[offset] * (0.2 + depth * 0.3)
                    buffer[offset] = filtered + buffer[offset] * (0.7 - depth * 0.2)
                end
                local channelBubble = bubble
                if channelIndex ~= 1 then
                    channelBubble = bubble * (0.7 + nextRandom() * 0.6)
                end
                local output = (filtered + reverb * 0.4 + channelBubble) * (1.0 - depth * 0.4)
                outputFrames[sampleIndex] = Engine.Clamp(output, -0.8, 0.8)
            end
            for index, delayFrames in ipairs(delays) do
                indices[index] = (indices[index] + 1) % delayFrames
            end
        end
        inputFrameCount.value = frameCount
        outputFrameCount.value = frameCount
    end
end

---@param cutoffFrequency? number
---@param transmission?    number
---@return sf.SoundSource.EffectProcessor
local function createBehindWall(cutoffFrequency, transmission)
    cutoffFrequency = cutoffFrequency == nil and 900.0 or cutoffFrequency
    transmission = transmission == nil and 0.35 or transmission
    ---@cast cutoffFrequency number
    ---@cast transmission number
    ---@type number?
    local alpha = nil
    local gain = Engine.Clamp(transmission, 0.0, 1.0)
    local firstStages = {}
    local secondStages = {}
    local filterChannelCount = 0

    local function initializeProcessor()
        local sampleRate = getPlaybackSampleRate()
        local maximumCutoff = math.max(20.0, sampleRate * 0.45)
        local cutoff = Engine.Clamp(cutoffFrequency, 20.0, maximumCutoff)
        alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / sampleRate)
    end

    local function resetFilterState(frameChannelCount)
        firstStages = {}
        secondStages = {}
        for channelIndex = 1, frameChannelCount do
            firstStages[channelIndex] = 0.0
            secondStages[channelIndex] = 0.0
        end
        filterChannelCount = frameChannelCount
    end

    return function (inputFrames, inputFrameCount, outputFrames, outputFrameCount, frameChannelCount)
        if inputFrames == nil or frameChannelCount == 0 then
            clearFrameCounts(inputFrameCount, outputFrameCount)
            return
        end
        if alpha == nil then
            initializeProcessor()
        end
        ---@cast alpha number
        if filterChannelCount ~= frameChannelCount then
            resetFilterState(frameChannelCount)
        end
        local frameCount = math.min(inputFrameCount.value, outputFrameCount.capacity)
        for frameIndex = 1, frameCount do
            local frameOffset = (frameIndex - 1) * frameChannelCount
            for channelIndex = 1, frameChannelCount do
                local sampleIndex = frameOffset + channelIndex
                local input = inputFrames[sampleIndex]
                local firstStage = firstStages[channelIndex] + alpha * (input - firstStages[channelIndex])
                local secondStage = secondStages[channelIndex] + alpha * (firstStage - secondStages[channelIndex])
                firstStages[channelIndex] = firstStage
                secondStages[channelIndex] = secondStage
                outputFrames[sampleIndex] = Engine.Clamp(secondStage * gain, -1.0, 1.0)
            end
        end
        inputFrameCount.value = frameCount
        outputFrameCount.value = frameCount
    end
end

local AudioEffects = {}

AudioEffects.EFFECTS = {
    Echo = createEcho,
    Distortion = createDistortion,
    Underwater = createUnderwater,
    BehindWall = createBehindWall
}

return AudioEffects
