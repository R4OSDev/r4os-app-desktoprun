const std = @import("std");
const r4os = @import("r4os");
const w = @import("desktop_launch");
const cli = @import("cli.zig");
const resolve = @import("resolve.zig");
pub fn r4_app_main(app: *r4os.App) i32 {
    const sys = app.system();
    const raw = std.mem.trimStart(u8, app.args(), " \t");
    const json_requested = raw.len >= 5 and std.ascii.eqlIgnoreCase(raw[0..5], "/JSON") and (raw.len == 5 or raw[5] == ' ' or raw[5] == '\t');
    const options = cli.parse(app.args()) catch |err| return failure(&sys, .syntax, @errorName(err), json_requested);
    if (options.help) {
        help(&sys);
        return 0;
    }
    if (options.open and !@import("r4std").init(app.startContext())) return failure(&sys, .resolution, "ResolverUnavailable", options.json);
    var request = options.key orelse w.Message{};
    if (options.key == null) resolve.target(&sys, options, &request) catch |err| return failure(&sys, .resolution, @errorName(err), options.json);
    const services = app.services() orelse return failure(&sys, .transport, "ServiceAPI", options.json);
    var connection = switch (services.open("WINSVC")) {
        .connection => |v| v,
        .failure => return failure(&sys, .no_desktop, "WINSVCUnavailable", options.json),
    };
    defer _ = connection.close();
    var service: r4os.abi.ProgramProcessHandle = .{};
    if (sys.programOpenHandle(connection.info.instance_id, &service) != 0) return failure(&sys, .transport, "BrokerIdentity", options.json);
    if (options.key == null) {
        var caller: r4os.abi.ProgramProcessHandle = .{};
        if (app.raw.instance_id == 0 or app.raw.instance_id > std.math.maxInt(u32) or sys.programOpenHandle(@intCast(app.raw.instance_id), &caller) != 0) return failure(&sys, .transport, "CallerIdentity", options.json);
        request.broker = .{ .instance = service.instance_id, .generation = service.generation };
        request.request = .{ .instance = caller.instance_id, .generation = caller.generation };
        request.flags = w.retain_evidence | (if (options.frame) @as(u32, w.wait_frame) else 0);
        request.deadline = sys.ticks() +| sys.ticksFromMilliseconds(options.wait_ms);
    }
    const op = if (options.key != null) w.status else w.submit;
    var result = call(&sys, &connection, op, request, if (options.key != null) sys.ticks() +| sys.ticksFromMilliseconds(options.wait_ms) else request.deadline);
    if (options.key == null and result.code == .ok and !result.done()) {
        // The single remaining wait uses the ORIGINAL absolute deadline.
        // No re-submit, polling loop or new timeout window after acceptance.
        var query = result;
        query.deadline = request.deadline;
        query.path = .{0} ** 160;
        query.args = .{0} ** 128;
        query.title = .{0} ** 48;
        result = call(&sys, &connection, w.wait, query, request.deadline);
    }
    report(&sys, result, options.json);
    return @intCast(@intFromEnum(result.code));
}
fn call(sys: *const r4os.r4sys.Context, connection: *r4os.app_services.ServiceConnection, op: u16, request: w.Message, deadline: u64) w.Message {
    var bytes: [w.size]u8 = undefined;
    var reply: [w.size]u8 = undefined;
    w.encode(&bytes, request);
    const now = sys.ticks();
    var failed = request;
    if (now >= deadline) {
        failed.code = .timeout;
        return failed;
    }
    const duration = r4os.time_contract.durationFromTicks(deadline - now, sys.timeState().monotonic_hz) catch {
        failed.code = .transport;
        return failed;
    };
    switch (connection.call(op, &bytes, &reply, r4os.time_contract.timeoutFinite(duration))) {
        .response => |response| {
            const value = w.decode(reply[0..response.bytes]) catch {
                failed.code = .invalid;
                return failed;
            };
            if (!value.sameKey(request)) {
                failed.code = .invalid;
                return failed;
            }
            return value;
        },
        .timed_out => failed.code = .timeout,
        .failure, .remote_failure => |raw| {
            failed.code = .transport;
            failed.raw = raw;
        },
    }
    return failed;
}
fn failure(sys: *const r4os.r4sys.Context, code: w.Code, label: []const u8, json: bool) i32 {
    var bytes: [256]u8 = undefined;
    const text = if (json) std.fmt.bufPrint(&bytes, "{{\"errorlevel\":{d},\"error\":\"{s}\",\"phase\":\"before_submit\"}}", .{ @intFromEnum(code), label }) catch "DESKRUN error" else std.fmt.bufPrint(&bytes, "DESKRUN: {s} (ERRORLEVEL {d}, before submit)", .{ label, @intFromEnum(code) }) catch "DESKRUN error";
    sys.println(text);
    return @intCast(@intFromEnum(code));
}
fn report(sys: *const r4os.r4sys.Context, value: w.Message, json: bool) void {
    var bytes: [768]u8 = undefined;
    const unavailable = value.flags & w.evidence_unavailable != 0;
    const retained = value.flags & w.retain_evidence != 0 and value.process.valid() and !unavailable;
    const evidence = if (unavailable) "unavailable" else if (retained) "retained" else if (value.flags & w.retain_evidence != 0) "pending" else "none";
    const text = if (json) std.fmt.bufPrint(&bytes, "{{\"request\":\"{d}:{d}:{d}:{d}\",\"phase\":\"{s}\",\"error\":\"{s}\",\"errorlevel\":{d},\"process_id\":{d},\"process_generation\":{d},\"window\":{d},\"content\":{d},\"raw\":{d},\"terminal\":{s},\"evidence_retained\":{s},\"evidence_unavailable\":{s}}}", .{ value.broker.instance, value.broker.generation, value.request.instance, value.request.generation, @tagName(value.phase), @tagName(value.code), @intFromEnum(value.code), value.process.instance, value.process.generation, value.window, value.content, value.raw, if (value.done()) "true" else "false", if (retained) "true" else "false", if (unavailable) "true" else "false" }) catch "DESKRUN output error" else std.fmt.bufPrint(&bytes, "DESKRUN request={d}:{d}:{d}:{d} phase={s} result={s} ERRORLEVEL={d} process={d}:{d} window={d} content={d} raw={d} evidence={s}", .{ value.broker.instance, value.broker.generation, value.request.instance, value.request.generation, @tagName(value.phase), @tagName(value.code), @intFromEnum(value.code), value.process.instance, value.process.generation, value.window, value.content, value.raw, evidence }) catch "DESKRUN output error";
    sys.println(text);
    if (!json and (value.code == .timeout or value.code == .transport)) sys.println("Use DESKRUN /STATUS:<request> to inspect this request. Do not submit it again automatically.");
}
fn help(sys: *const r4os.r4sys.Context) void {
    sys.println("DESKRUN [/JSON] [/WAIT:ms] [/WAITFRAME] <app.R4X> [application arguments]");
    sys.println("DESKRUN [/JSON] [/WAIT:ms] [/WAITFRAME] /OPEN <folder, file or shortcut>");
    sys.println("DESKRUN [/JSON] /STATUS:<broker:gen:request:gen>");
    sys.println("Options precede the target. Quote paths containing spaces. Relative paths use CWD.");
    sys.println("Starts in the existing Desktop; the app survives this helper and SSH logout.");
    sys.println("WAIT defaults to 5000 ms (1..300000), shared by submission and window wait.");
    sys.println("WAITFRAME additionally waits for Desktop-consumed content, not physical display visibility.");
    sys.println("ERRORLEVEL: 0 OK, 1 transport, 2 syntax, 3 resolution, 4 no Desktop, 5 queue full,");
    sys.println("6 windows full, 7 loader, 8 attach, 9 timeout, 10 Desktop lost, 11 app ended,");
    sys.println("12 protocol, 13 unknown/expired request, 14 service restarted. No automatic re-submit.");
}
