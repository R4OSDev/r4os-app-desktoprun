# DESKRUN

Launch an application in the running local R4OS Desktop from a console or SSH session.

Build from this component directory inside the R4OS workspace with `./Build.sh` (Windows: `Build.bat`). Dependencies: SDK, Contract, Libraries and Services/WindowService.
See DOCUMENTATION.de.txt for commands and the ownership/wait contract.

DESKRUN2 requests generation-bound completion/output retention. JSON adds
`evidence_retained` / `evidence_unavailable`; `terminal` still completes the
launch request. Bind an independent SDK108/R4SYS32 observation before the
16-entry/60s broker history retires. The full contract and KVM4 qualification
are in `Docs/Desktop/DeskRun.txt`; explicit public C fixtures are under
`Tests/Guest/Observation` and `Tests/Guest/GuiOutput`.
