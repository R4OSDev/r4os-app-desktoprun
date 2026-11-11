//! Native link helper. The record is written and byte-read back before any
//! Activate. Separate invocations only observe or release this saved request.
const std = @import("std");
const r4os = @import("r4os");
const cli = @import("cli.zig");
const links = r4os.desktop_links;
fn absolute(sys: *const r4os.r4sys.Context, value: []const u8) !r4os.path.AbsoluteFilePath {
    var cwd: [r4os.path.file_path_max + 1]u8 = undefined;
    const got = sys.envGet("CWD", &cwd);
    return cli.absolute(value, if (got > 0 and got < cwd.len) cwd[0..@intCast(got)] else "C:\\");
}
fn save(sys: *const r4os.r4sys.Context, path: r4os.path.AbsoluteFilePath, q: links.Request) bool {
    const bytes = std.mem.asBytes(&q);
    if (sys.fileWrite(path.asZ().ptr, bytes) != @as(i32, @intCast(bytes.len))) return false;
    var check: links.Request = .{};
    return sys.fileRead(path.asZ().ptr, std.mem.asBytes(&check)) == @as(i32, @intCast(bytes.len)) and std.meta.eql(q, check);
}
fn result(output: anytype, call: links.Call) i32 {
    const r = switch (call) {
        .reply => |value| value,
        .failure => |code| {
            var bytes: [96]u8 = undefined;
            output.println(std.fmt.bufPrint(&bytes, "{{\"transport\":{d},\"reconcile_saved_original\":true}}", .{code}) catch "Link transport error");
            return 1;
        },
    };
    var bytes: [720]u8 = undefined;
    output.println(std.fmt.bufPrint(&bytes, "{{\"result\":{d},\"phase\":{d},\"serial\":\"{d}\",\"process_id\":{d},\"process_generation\":\"{d}\",\"accepted_tick\":\"{d}\",\"started_tick\":\"{d}\",\"ready_tick\":\"{d}\",\"content\":\"{d}\",\"flags\":{d},\"exit_code\":{d},\"cleanup_tick\":\"{d}\"}}", .{ r.result, r.phase, r.request.token.serial, r.process.instance_id, r.process.generation, r.accepted_tick, r.started_tick, r.ready_tick, r.content_generation, r.flags, r.completion.exit_code, r.cleanup_tick }) catch "Link receipt too large");
    return if (r.result == 0) 0 else 1;
}
pub fn run(app: *r4os.App2, raw: []const u8, output: anytype) ?i32 {
    var failure: r4os.arguments.Failure = .{};
    var cursor = r4os.arguments.ArgCursor.init(raw, .{}, &failure) catch return null;
    const first = (cursor.next() catch return null) orelse return null;
    const command = first.bytes(raw) catch return null;
    const activate = std.ascii.eqlIgnoreCase(command, "/LINK");
    const status = std.ascii.eqlIgnoreCase(command, "/LINKSTATUS");
    const release = std.ascii.eqlIgnoreCase(command, "/LINKRELEASE");
    if (!activate and !status and !release) return null;
    const sys = app.system();
    const record_arg = ((cursor.next() catch return 2) orelse return 2).bytes(raw) catch return 2;
    const record = absolute(&sys, record_arg) catch return 2;
    const client = links.Client{ .sys = sys };
    if (!activate) {
        if ((cursor.remainingArguments() catch return 2).len != 0) return 2;
        var saved: links.Request = .{};
        if (sys.fileRead(record.asZ().ptr, std.mem.asBytes(&saved)) != @sizeOf(links.Request)) return 3;
        return result(output, if (status) client.status(saved) else client.release(saved));
    }
    const path_arg = ((cursor.next() catch return 2) orelse return 2).bytes(raw) catch return 2;
    if ((cursor.remainingArguments() catch return 2).len != 0) return 2;
    const path = absolute(&sys, path_arg) catch return 2;
    // Never overwrite an existing intent file with a new activation. Its
    // original saved record is the sole recovery path after an uncertain reply.
    if (sys.fileInfo(record.asZ().ptr) != null) {
        output.println("Saved link intent exists; use /LINKSTATUS with that file.");
        return 3;
    }
    var bytes: [1025]u8 = undefined;
    const count = sys.fileRead(path.asZ().ptr, &bytes);
    if (count <= 0 or count > r4os.abi.desktop_link_max_bytes) return 3;
    const scope = switch (client.scope()) {
        .reply => |r| r,
        .failure => return 1,
    };
    if (!links.supported(&scope)) {
        output.println("Native Desktop link activation unavailable.");
        return 4;
    }
    var owner: r4os.abi.ProgramProcessHandle = .{};
    const instance = std.math.cast(u32, app.raw.instance_id) orelse return 4;
    if (instance == 0 or sys.programOpenHandle(instance, &owner) != 0) return 4;
    // One helper owns one activation. This is a logical deduplication key,
    // not entropy: freshness across service/boot is the broker's real epoch.
    const intent = [3]u64{ owner.generation, sys.ticks(), owner.instance_id };
    const q = links.binding(scope.request.token, owner, intent, sys.ticks() +| sys.ticksFromMilliseconds(30000), path.bytes(), bytes[0..@intCast(count)]) orelse return 2;
    if (!save(&sys, record, q)) return 3;
    const prepared = switch (client.prepare(q)) {
        .reply => |r| r,
        .failure => return 1,
    };
    if (prepared.result != 0) return result(output, .{ .reply = prepared });
    if (prepared.request.token.serial == 0 or !save(&sys, record, prepared.request)) return 3;
    return result(output, client.activate(prepared.request));
}
