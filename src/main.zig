const std = @import("std");
const r4os = @import("r4os");
const w = @import("desktop_launch");
const cli = @import("cli.zig");
const resolve = @import("resolve.zig");
const ControlGrant = struct { requested: bool = false, result: i32 = 0, serial: u64 = 0 };
const Output = struct {
    sys: r4os.r4sys.Context,
    stdio: r4os.launch_io.Stdio,
    failed: bool = false,

    pub fn println(self: *Output, text: []const u8) void {
        const a = r4os.abi;
        const limit: r4os.byte_pipe.Deadline = if (self.stdio.snapshot.stdout_binding.kind == a.launch_io_pipe)
            .{ .kind = a.timeout_kind_forever }
        else
            .{};
        for ([_][]const u8{ text, "\r\n" }) |part| {
            var remaining = part;
            while (remaining.len != 0 and !self.failed) {
                var progress: r4os.byte_pipe.Progress = .{};
                const rc = self.stdio.write(1, remaining, limit, &progress);
                if (rc != 0 or progress.state != a.byte_pipe_ready or progress.bytes == 0 or progress.bytes > remaining.len) {
                    self.failed = true;
                    return;
                }
                remaining = remaining[@intCast(progress.bytes)..];
            }
        }
    }
};

pub fn r4_app_main(app: *r4os.App2) i32 {
    var output: Output = .{ .sys = app.system(), .stdio = undefined };
    const rc = app.stdio(&output.stdio);
    if (rc != 0) return rc;
    defer for (0..3) |stream| {
        _ = output.stdio.close(@intCast(stream));
    };
    const result = run(app, &output);
    return if (result == 0 and output.failed) 1 else result;
}

fn run(app: *r4os.App2, output: *Output) i32 {
    const sys = output.sys;
    const raw = std.mem.trimStart(u8, app.args(), " \t");
    const json_requested = raw.len >= 5 and std.ascii.eqlIgnoreCase(raw[0..5], "/JSON") and (raw.len == 5 or raw[5] == ' ' or raw[5] == '\t');
    if (@import("links.zig").run(app, raw, output)) |code| return code;
    const options = cli.parse(app.args()) catch |err| return failure(output, .syntax, @errorName(err), json_requested);
    if (options.help) {
        help(output);
        return 0;
    }
    if (options.open and !@import("r4std").init2(app.raw)) return failure(output, .resolution, "ResolverUnavailable", options.json);
    var request = options.key orelse w.Message{};
    if (options.key == null) resolve.target(&sys, options, &request) catch |err| return failure(output, .resolution, @errorName(err), options.json);
    const services = app.services() orelse return failure(output, .transport, "ServiceAPI", options.json);
    var connection = switch (services.open("WINSVC")) {
        .connection => |v| v,
        .failure => return failure(output, .no_desktop, "WINSVCUnavailable", options.json),
    };
    defer _ = connection.close();
    var service: r4os.abi.ProgramProcessHandle = .{};
    var detail: r4os.abi.ServiceDetail = .{};
    if (sys.serviceBoundDetail("WINSVC", &detail, &service) != 0 or
        service.instance_id != connection.info.instance_id or service.generation == 0 or service.reserved != 0)
        return failure(output, .transport, "BrokerIdentity", options.json);
    const clock = app.clock() catch return failure(output, .transport, "ClockUnavailable", options.json);
    var budget = r4os.rpc_budget.Budget.fromTicks(clock.read, sys.ticksFromMilliseconds(options.wait_ms)) catch |err|
        return failure(output, .transport, @errorName(err), options.json);
    if (options.key == null) {
        var caller: r4os.abi.ProgramProcessHandle = .{};
        if (app.raw.instance_id == 0 or app.raw.instance_id > std.math.maxInt(u32) or sys.programOpenHandle(@intCast(app.raw.instance_id), &caller) != 0) return failure(output, .transport, "CallerIdentity", options.json);
        request.broker = .{ .instance = service.instance_id, .generation = service.generation };
        request.request = .{ .instance = caller.instance_id, .generation = caller.generation };
        request.flags = w.retain_evidence | (if (options.frame) @as(u32, w.wait_frame) else 0);
        request.deadline = budget.tick_deadline;
    }
    // Bind the original request before submission. A later numeric /STATUS
    // cannot reconstruct a lost boot/service epoch and never invents one.
    const scope = if (options.key == null) evidenceScope(&sys, &connection, request, &budget) else null;
    const op = if (options.key != null) w.status else w.submit;
    var result = call(&connection, op, request, &budget);
    var control: ControlGrant = .{ .requested = options.controller != null, .result = if (options.controller != null) r4os.abi.desktop_action_result_unavailable else 0 };
    if (options.controller) |controller| if (options.key == null and result.code == .ok) {
        if (scope) |original| control = grantController(&sys, &connection, request, original, controller, &budget);
    };
    if (options.key == null and result.code == .ok and !result.done()) {
        // The single remaining wait uses the ORIGINAL absolute deadline.
        // No re-submit, polling loop or new timeout window after acceptance.
        var query = result;
        query.deadline = request.deadline;
        query.path = .{0} ** 160;
        query.args = .{0} ** 128;
        query.title = .{0} ** 48;
        result = call(&connection, w.wait, query, &budget);
    }
    report(output, result, options.json, scope, control);
    return @intCast(@intFromEnum(result.code));
}
fn evidenceScope(sys: *const r4os.r4sys.Context, connection: *r4os.app_services.ServiceConnection, launch: w.Message, budget: *r4os.rpc_budget.Budget) ?r4os.abi.DesktopEvidenceToken {
    const a = r4os.abi;
    const remaining = budget.remainingTicks() catch return null;
    const request: a.DesktopEvidenceRequest = .{};
    var reply: a.DesktopEvidenceReply = .{};
    const ticks = @min(remaining, sys.ticksFromMilliseconds(500));
    switch (r4os.app_services.callWithTicks(connection, a.desktop_evidence_op_scope, std.mem.asBytes(&request), std.mem.asBytes(&reply), ticks)) {
        .response => |got| {
            if (got.bytes != @sizeOf(a.DesktopEvidenceReply) or reply.result != 0 or reply.token.version != 1 or reply.token.size != @sizeOf(a.DesktopEvidenceToken) or
                reply.token.broker.instance_id != launch.broker.instance or reply.token.broker.generation != launch.broker.generation or
                reply.token.epoch[0] | reply.token.epoch[1] | reply.token.epoch[2] == 0) return null;
            return reply.token;
        },
        else => return null,
    }
}
fn grantController(sys: *const r4os.r4sys.Context, connection: *r4os.app_services.ServiceConnection, launch: w.Message, scope: r4os.abi.DesktopEvidenceToken, controller: r4os.abi.ProgramProcessHandle, budget: *r4os.rpc_budget.Budget) ControlGrant {
    const a = r4os.abi;
    const remaining = budget.remainingTicks() catch return .{ .requested = true, .result = a.desktop_action_result_unknown };
    const token: a.DesktopActionToken = .{ .window = .{ .service = scope.broker }, .request = .{ .instance_id = launch.request.instance, .generation = launch.request.generation }, .controller = controller, .epoch = scope.epoch, .rights = a.desktop_action_right_all };
    const request: a.DesktopActionRequest = .{ .token = token };
    var reply: a.DesktopActionReply = .{};
    switch (r4os.app_services.callWithTicks(connection, a.desktop_action_op_grant, std.mem.asBytes(&request), std.mem.asBytes(&reply), @min(remaining, sys.ticksFromMilliseconds(500)))) {
        .response => |got| {
            const value = reply.request.token;
            if (got.bytes != @sizeOf(a.DesktopActionReply) or value.version != 1 or value.size != @sizeOf(a.DesktopActionToken) or
                !std.meta.eql(value.window.service, token.window.service) or !std.meta.eql(value.request, token.request) or
                !std.meta.eql(value.controller, token.controller) or !std.meta.eql(value.epoch, token.epoch) or value.rights != token.rights or
                (reply.result == 0 and value.grant == 0)) return .{ .requested = true, .result = a.desktop_action_result_invalid };
            return .{ .requested = true, .result = reply.result, .serial = value.grant };
        },
        else => return .{ .requested = true, .result = a.desktop_action_result_unknown },
    }
}
fn call(connection: *r4os.app_services.ServiceConnection, op: u16, request: w.Message, budget: *r4os.rpc_budget.Budget) w.Message {
    var bytes: [w.size]u8 = undefined;
    var reply: [w.size]u8 = undefined;
    w.encode(&bytes, request);
    var failed = request;
    switch (connection.callBudget(op, &bytes, &reply, budget)) {
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
fn failure(output: *Output, code: w.Code, label: []const u8, json: bool) i32 {
    var bytes: [256]u8 = undefined;
    const text = if (json) std.fmt.bufPrint(&bytes, "{{\"errorlevel\":{d},\"error\":\"{s}\",\"phase\":\"before_submit\"}}", .{ @intFromEnum(code), label }) catch "DESKRUN error" else std.fmt.bufPrint(&bytes, "DESKRUN: {s} (ERRORLEVEL {d}, before submit)", .{ label, @intFromEnum(code) }) catch "DESKRUN error";
    output.println(text);
    return @intCast(@intFromEnum(code));
}
fn report(output: *Output, value: w.Message, json: bool, scope: ?r4os.abi.DesktopEvidenceToken, control: ControlGrant) void {
    var bytes: [1024]u8 = undefined;
    const epoch = if (scope) |bound| bound.epoch else @as([3]u64, .{0} ** 3);
    const unavailable = value.flags & w.evidence_unavailable != 0;
    const retained = value.flags & w.retain_evidence != 0 and value.process.valid() and !unavailable;
    const evidence = if (unavailable) "unavailable" else if (retained) "retained" else if (value.flags & w.retain_evidence != 0) "pending" else "none";
    const text = if (json) std.fmt.bufPrint(&bytes, "{{\"request\":\"{d}:{d}:{d}:{d}\",\"phase\":\"{s}\",\"error\":\"{s}\",\"errorlevel\":{d},\"process_id\":{d},\"process_generation\":{d},\"window\":{d},\"content\":{d},\"raw\":{d},\"terminal\":{s},\"evidence_retained\":{s},\"evidence_unavailable\":{s},\"evidence_scope_available\":{s},\"evidence_epoch\":[\"{d}\",\"{d}\",\"{d}\"],\"control_requested\":{s},\"control_result\":{d},\"control_grant\":\"{d}\"}}", .{ value.broker.instance, value.broker.generation, value.request.instance, value.request.generation, @tagName(value.phase), @tagName(value.code), @intFromEnum(value.code), value.process.instance, value.process.generation, value.window, value.content, value.raw, if (value.done()) "true" else "false", if (retained) "true" else "false", if (unavailable) "true" else "false", if (scope != null) "true" else "false", epoch[0], epoch[1], epoch[2], if (control.requested) "true" else "false", control.result, control.serial }) catch "DESKRUN output error" else std.fmt.bufPrint(&bytes, "DESKRUN request={d}:{d}:{d}:{d} phase={s} result={s} ERRORLEVEL={d} process={d}:{d} window={d} content={d} raw={d} evidence={s}", .{ value.broker.instance, value.broker.generation, value.request.instance, value.request.generation, @tagName(value.phase), @tagName(value.code), @intFromEnum(value.code), value.process.instance, value.process.generation, value.window, value.content, value.raw, evidence }) catch "DESKRUN output error";
    output.println(text);
    if (!json and control.requested) {
        const detail = std.fmt.bufPrint(&bytes, "DESKRUN control grant={d} result={d}; launch ERRORLEVEL and control result are separate.", .{ control.serial, control.result }) catch "DESKRUN control unknown";
        output.println(detail);
    }
    if (!json and (value.code == .timeout or value.code == .transport)) output.println("Use DESKRUN /STATUS:<request> to inspect this request. Do not submit it again automatically.");
}
fn help(output: *Output) void {
    output.println("DESKRUN [/JSON] [/WAIT:ms] [/WAITFRAME] [/CONTROLLER:pid:generation] <app.R4X> [arguments]");
    output.println("DESKRUN [/JSON] [/WAIT:ms] [/WAITFRAME] /OPEN <folder, file or shortcut>");
    output.println("DESKRUN [/JSON] /STATUS:<broker:gen:request:gen>");
    output.println("DESKRUN /LINK <new-record-file> <own-shortcut.LNK>");
    output.println("DESKRUN /LINKSTATUS <saved-record-file> | /LINKRELEASE <saved-record-file>");
    output.println("LINK saves the original binding before activation. After response loss use LINKSTATUS; it never starts again.");
    output.println("Options precede the target. Quote paths containing spaces. Relative paths use CWD.");
    output.println("Starts in the existing Desktop; the app survives this helper and SSH logout.");
    output.println("WAIT defaults to 5000 ms (1..300000), shared by submission and window wait.");
    output.println("WAITFRAME additionally waits for Desktop-consumed content, not physical display visibility.");
    output.println("CONTROLLER explicitly delegates Resize/Minimize/Restore to that live program. Check control_result/control_grant separately.");
    output.println("ERRORLEVEL: 0 OK, 1 transport, 2 syntax, 3 resolution, 4 no Desktop, 5 queue full,");
    output.println("6 windows full, 7 loader, 8 attach, 9 timeout, 10 Desktop lost, 11 app ended,");
    output.println("12 protocol, 13 unknown/expired request, 14 service restarted. No automatic re-submit.");
}
