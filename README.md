# Minimize to tray

This program minimizes a window to the taskbar notification area (also informally known as the *system tray*).

This might be useful for long-running programs that you are happy to leave in the background and forget about.

This is designed to be scriptable -- if you want something more interactive/point-and-click, you might prefer: [RBTray](http://rbtray.sourceforge.net/).

Usage:

```bat
minimize-to-tray [/NOMIN|/MIN] "title"
```

Where:

* `"title"` - a window title to find by exact match (you should place the title in quotes if it has spaces or other special characters).  
  * To search for any matching text within the title, place an asterisk either side (e.g. `"*watch*"` matches `"C:\WINDOWS\system32\cmd.exe - wsl watch ps"`).
  * To search for a matching suffix, place an asterisk before the title (e.g. `"*Notepad"` matches `"Untitled - Notepad"`).  
  * To search for a matching prefix, place an asterisk after the title (e.g. `"Signal*"` matches `"Signal (1)"`).  
* `/NOMIN` - does not immediately hide the window (waits until you to manually minimize it).
* `/MIN` - immediately hides the window (default).

Effect:

* The window that has a title matching the given suffix will appear to be minimized as an icon in the taskbar notification area (system tray).
* The notification icon and tooltip should match the window's icon and title.
* Left-clicking on the notification icon restores the window.
* When you minimize the window yourself, it will disappear back to the taskbar notification area.
* Right-clicking on the notification icon shows a menu:
  * *Restore* - restores the minimized window.  When you minimize it again, it will return to the notification area.
  * *Close* - sends the window a close signal (it restores it first in case it needs to show UI).
  * *Stop Hiding* - takes the window out of the notification area (it will now be minimized normally) and does not track any future minimizes.  This closes this utility for that window.


## Troubleshooting

* Some windows (e.g. _modern_ UWP apps from Windows Store) don't properly give their icon through the standard API, so a default one from the app is used.

* If using a terminal, your current command could be used as the window's title, so it could match the window being found -- just place a space after the command, or put the title in quotes to prevent this.

* The window must exist before running the utility, so you may need delay during any automated scripts. If using a batch file, you could try something like: `CHOICE /C 0 /D 0 /T 5 >NUL`

* You should not need to, but if you have any issues where your application does not automatically go back to the tray when it is later minimized, you can force a hacky *polling mode* with the option `/POLL`.

* If there are any old instances of this tool running, you can clean them up by running the following (do not use the 'force' mode as any tracked application windows will stay hidden!): `taskkill /im minimize-to-tray.exe`

* Additional output for debugging can be seen with these options (which must be the first option given):

  * `/CONSOLE:ATTACH` - attach stdio to the parent console
  * `/CONSOLE:CREATE` - create a console (if 'attach' also specified, then only if that fails)
  * `/CONSOLE:ATTACH-CREATE` - attach stdio to the parent console but, if that fails, create a console

* If the history of the taskbar notification area (*Taskbar settings* / *Select which icons appear on the taskbar*) becomes too polluted with old entries, you can manually clear the cache with the following command (WARNING: this restarts Windows Explorer so you may lose some Explorer windows): 

```bat
reg delete "HKCU\Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\TrayNotify" /v PastIconsStream /f && taskkill /im explorer.exe /f && start "Restarting" /d "%systemroot%" /i /normal explorer.exe
```
