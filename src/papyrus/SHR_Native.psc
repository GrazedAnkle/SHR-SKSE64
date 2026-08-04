Scriptname SHR_Native Hidden
{Papyrus-side bridge into the SHR SKSE plugin.

Holds nothing but native declarations, so it compiles with no import path: no SkyUI, no MCM Helper,
no Bethesda base sources. The setting surface is generic, keyed by the control ids in
Data/MCM/Config/SHR/config.json, so adding a setting is a layout edit and a row in the registry at
src/adapter/Settings.hpp - neither this script nor SHR_ConfigMenu changes.

Ids that the registry does not know are rejected and logged; the plugin never invents a setting.}

; --- Writes. Invoked by a config.json action, which resolves {value} into the argument. There is
; --- one per Papyrus type a control can carry, because the token is typed at the call site.
Function SetFloat(string a_id, float a_value) global native
Function SetBool(string a_id, bool a_value) global native
Function SetInt(string a_id, int a_value) global native

; --- Reads, for display sync. The plugin's own store is authoritative: MCM Helper's is profile-wide
; --- and so cannot express a per-character value.
float Function GetFloat(string a_id) global native
bool Function GetBool(string a_id) global native
int Function GetInt(string a_id) global native

; --- The ids to sync, grouped by the setter each one needs. Returned by the plugin rather than
; --- listed here, which is what keeps a new setting from touching this file.
string[] Function GetFloatIds() global native
string[] Function GetBoolIds() global native
string[] Function GetIntIds() global native

; --- Reset. Each scope falls back to the layer beneath it: a character to the configuration
; --- profile, the profile to the values compiled in. Callers must resync the display afterwards.
Function ResetSubject() global native
Function ResetProfile() global native
