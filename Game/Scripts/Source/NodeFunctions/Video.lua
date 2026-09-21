local Video = {}

function Video.PlayVideo(videoFileName, mute, skipable)
    mute = mute == nil and false or mute
    skipable = skipable == nil and true or skipable
    _G.playVideo(videoFileName, mute, skipable)
end

return Video
