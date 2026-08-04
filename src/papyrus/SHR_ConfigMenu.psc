Scriptname SHR_ConfigMenu extends MCM_ConfigBase
{Host script for SHR's Mod Configuration Menu entry, attached to the quest in SHR.esp.

The menu's layout, ranges, labels and help text live in Data/MCM/Config/SHR/config.json, not here.
This script exists because MCM Helper resolves a menu through SkyUI's registered config quests, so
something has to be attached to the quest, and because OnConfigOpen and RefreshMenu are only
reachable from a config instance.

It holds no setting names. Every id comes from the plugin, so this script - the only one needing
MCM Helper's, SkyUI's and Bethesda's sources on the import path - does not change when a setting is
added. Keep it that way.}

Event OnConfigOpen()
    SyncDisplay()
EndEvent

Event OnSettingChange(string a_ID)
    ; The control's own action has already written the value and fires ~7ms ahead of this event, so
    ; this is a read-back, not a second apply: it shows what the plugin stored rather than what the
    ; control was dragged to, which is how a clamped or rejected edit corrects itself on screen.
    SyncDisplay()
    RefreshMenu()
EndEvent

; Reached by a config.json CallFunction action naming this quest, because a reset has to repaint and
; RefreshMenu is only callable on a config instance. Still setting-agnostic: the plugin decides what
; a default is, and SyncDisplay discovers what to redraw.
Function ResetCharacterSettings()
    SHR_Native.ResetSubject()
    SyncDisplay()
    RefreshMenu()
EndFunction

Function ResetGeneralSettings()
    SHR_Native.ResetProfile()
    SyncDisplay()
    RefreshMenu()
EndFunction

Function SyncDisplay()
    string[] ids = SHR_Native.GetFloatIds()
    int i = 0
    While i < ids.Length
        SetModSettingFloat(ids[i], SHR_Native.GetFloat(ids[i]))
        i += 1
    EndWhile

    ids = SHR_Native.GetBoolIds()
    i = 0
    While i < ids.Length
        SetModSettingBool(ids[i], SHR_Native.GetBool(ids[i]))
        i += 1
    EndWhile

    ids = SHR_Native.GetIntIds()
    i = 0
    While i < ids.Length
        SetModSettingInt(ids[i], SHR_Native.GetInt(ids[i]))
        i += 1
    EndWhile
EndFunction
