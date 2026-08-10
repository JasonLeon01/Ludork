on run arguments
    if (count of arguments) is not 1 then error "A mounted DMG path is required."
    set volumeFolder to POSIX file (item 1 of arguments) as alias

    tell application "Finder"
        open volumeFolder
        repeat with attempt from 1 to 20
            try
                set volumeWindow to container window of volumeFolder
                if current view of volumeWindow is not icon view then
                    set current view of volumeWindow to icon view
                    delay 0.5
                    set volumeWindow to container window of volumeFolder
                end if
                set the bounds of volumeWindow to {100, 100, 860, 640}
                exit repeat
            on error errorMessage number errorNumber
                if attempt is 20 then error errorMessage number errorNumber
                delay 0.5
            end try
        end repeat
        try
            set toolbar visible of volumeWindow to false
        end try
        try
            set statusbar visible of volumeWindow to false
        end try
        try
            set pathbar visible of volumeWindow to false
        end try
        try
            set sidebar width of volumeWindow to 0
        end try
        set viewOptions to the icon view options of volumeWindow
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 96
        set text size of viewOptions to 14
        set background picture of viewOptions to file ".background:background.png" of volumeFolder
        set position of item "Ludork.app" of volumeFolder to {180, 190}
        set position of item "Applications" of volumeFolder to {580, 190}
        set position of item "Install Official Plugins.command" of volumeFolder to {580, 375}
        try
            set position of item ".background" of volumeFolder to {1000, 1000}
            set position of item ".official-plugin-payload" of volumeFolder to {1050, 1050}
            set position of item ".VolumeIcon.icns" of volumeFolder to {1100, 1100}
            set position of item ".metadata_never_index" of volumeFolder to {1150, 1150}
        end try
        update volumeFolder without registering applications
        delay 2
        try
            close container window of volumeFolder
        end try
    end tell
end run
