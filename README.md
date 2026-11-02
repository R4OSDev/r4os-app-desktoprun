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

DESKRUN 3 adds `evidence_scope_available` and lossless decimal-string
`evidence_epoch[3]` to the original launch JSON. The scope is queried from the
same actual broker before Submit, within the original wait deadline. Save
that epoch together with the full broker/request key for SDK126 evidence
reservation/Offer/ACK/Status. A legacy numeric /STATUS has no saved epoch;
it reports scope unavailable and cannot establish boot-safe resumption.
Use the original saved SDK token for that read-only reconciliation.
Normal launchV1 and old JSON fields remain compatible. Candidate native97
qualification is pending; Studio0.8.1 qualification is not claimed.

0.88.97 native qualification: DESKRUN3 preserves the original pre-submit
WINSVC epoch as lossless decimal strings. Launch terminal/window success does
not mean process completion; read the retained original completion separately.
Normal Kernel452/SMP4/Desktop197/WINSVC29, SDK126/Contract67 source candidates:
actual C/Rust/Zig saved-token handoff, history/expiry/last release and a fresh
boot with all numeric identities repeated pass. A numeric legacy STATUS key
cannot reconstruct the old epoch. See Docs/Desktop/DeskRun.txt; published
matching and external Studio qualification remain separate final steps.

0.88.98 / DESKRUN4 adds `/CONTROLLER:pid:generation` before the target.
The original helper grants only its known accepted launch within the remaining
original deadline, using the service epoch captured before submission.
JSON adds control_requested, control_result and lossless control_grant; check
control_result separately from unchanged launch ERRORLEVEL semantics.
The named controller resolves the canonical token by status and can request
real resize/minimize/restore with an immutable operation and absolute deadline.
This does not transfer reap rights. Without the option no grant is created.
Legacy launch.window remains slot+1; the new action token uses the native
zero-based GUI index. WINSVC31 adapts the two contracts. Native exact-child
actions/frame/input/slotreuse and public C/Rust/Zig pass with sourceSDK127,
Contract68, Desktop198 and unchangedKernel452/SMP4. Final published matching
remains0.88.99/100; this does not qualify Studio0.8.3 or publish a release.

DESKRUN 0.1.5 adds native own-link activation with `/LINK record-file own.LNK`,
`/LINKSTATUS record-file` and `/LINKRELEASE record-file`. The helper persists
and verifies the original generation/epoch/content-bound request before
activation. Lost replies are reconciled by read-only Status. The actual
Desktop resolves the real link, and its process/ready/end/cleanup receipt is
available through the typed Zig/C/Rust SDK API. See DOCUMENTATION.de.txt.
