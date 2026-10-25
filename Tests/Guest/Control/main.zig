//! Explicit disposable-guest control. Closes only an exact handle supplied
//! by the test's DESKRUN receipt; Desktop retains normal child reap ownership.
const std = @import("std");
const r4os = @import("r4os");
pub fn r4_app_main(app: *r4os.App) i32 {
    const sys = app.system();
    const args = app.args();
    if (std.mem.eql(u8, args, "/CHILD")) {
        sys.println("DESKRUN console child is visible and independent of SSH");
        while (!sys.programShouldClose()) sys.sleepTicks(10);
        return 0;
    }
    var parts = std.mem.tokenizeAny(u8, args, " :");
    const op = parts.next() orelse return 1;
    const id = std.fmt.parseInt(u32, parts.next() orelse return 1, 10) catch return 1;
    const generation = std.fmt.parseInt(u64, parts.next() orelse return 1, 10) catch return 1;
    if (id == 0 or generation == 0 or parts.next() != null) return 1;
    const handle = r4os.abi.ProgramProcessHandle{ .instance_id = id, .generation = generation };
    var info: r4os.abi.ProgramInstanceInfo = .{};
    if (std.mem.eql(u8, op, "CLOSE")) {
        if (sys.programHandleRequestClose(&handle) != 0) return 2;
        const deadline = sys.ticks() +| sys.ticksFromMilliseconds(8000);
        while (sys.ticks() < deadline) {
            const result = sys.programHandleStatus(&handle, &info);
            if (result == r4os.abi.program_handle_error_not_found or result == r4os.abi.program_handle_error_stale) {
                sys.println("CLOSED and reaped exact process");
                return 0;
            }
            sys.sleepTicks(1);
        }
        sys.println("Close requested; exact reap still pending");
        return 3;
    }
    if (!std.mem.eql(u8, op, "STATUS")) return 1;
    const result = sys.programHandleStatus(&handle, &info);
    var bytes: [256]u8 = undefined;
    sys.println(std.fmt.bufPrint(&bytes, "{{\"status\":{d},\"id\":{d},\"generation\":{d},\"state\":{d},\"window\":{d},\"class\":{d}}}", .{ result, id, generation, info.state, info.window_id, info.app_class }) catch return 4);
    return if (result == 0) 0 else 5;
}
