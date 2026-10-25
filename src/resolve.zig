//! Same R4STD resolver as Explorer: no DESKRUN extension table or ROM paths.
const std = @import("std");
const r4os = @import("r4os");
const r4std = @import("r4std");
const w = @import("desktop_launch");
const cli = @import("cli.zig");
const explorer = "C:\\R4OS\\SOFTWARE\\DESKTOP\\EXPLORER.R4X";
pub fn target(sys: *const r4os.r4sys.Context, options: cli.Options, message: *w.Message) !void {
    var cwd: [r4os.path.file_path_max + 1]u8 = undefined;
    const count = sys.envGet("CWD", &cwd);
    const base = if (count > 0 and count <= cwd.len) cwd[0..@intCast(count)] else "";
    const path = try cli.absolute(options.target, base);
    if (!options.open) {
        if (!r4std.app_assoc.isR4XPath(path.bytes())) return error.ExpectedR4X;
        try w.copy(&message.path, path.bytes());
        try w.copy(&message.args, options.args);
        return;
    }
    const info = sys.fileInfo(path.asZ().ptr) orelse return error.NotFound;
    if (info.is_dir != 0) return directory(path.bytes(), message);
    if (std.ascii.endsWithIgnoreCase(path.bytes(), ".LNK")) {
        var bytes: [4096]u8 = undefined;
        const got = sys.fileRead(path.asZ().ptr, &bytes);
        if (got <= 0 or got >= bytes.len) return error.InvalidShortcut;
        const link = try r4std.shortcut.parse(bytes[0..@intCast(got)]);
        const resolved = try link.resolve();
        switch (resolved.kind) {
            .program => {
                try w.copy(&message.path, resolved.target);
                try w.copy(&message.args, resolved.args);
                message.policy = @intFromEnum(resolved.policy);
                // Caption only; program/argument bytes are never shortened.
                const n = @min(resolved.title.len, message.title.len - 1);
                var end = n;
                while (end > 0 and !std.unicode.utf8ValidateSlice(resolved.title[0..end])) end -= 1;
                try w.copy(&message.title, resolved.title[0..end]);
            },
            .directory => try directory(resolved.target, message),
            .file => try file(sys, resolved.target, message),
        }
        return;
    }
    try file(sys, path.bytes(), message);
}
fn directory(path: []const u8, message: *w.Message) !void {
    try w.copy(&message.path, explorer);
    try w.copy(&message.args, path);
    message.policy = @intFromEnum(r4os.abi.LaunchPolicy.gui);
}
fn file(sys: *const r4os.r4sys.Context, path: []const u8, message: *w.Message) !void {
    var assoc: r4std.app_assoc.Config = undefined;
    assoc.loadDefaults();
    var bytes: [4096]u8 = undefined;
    const got = sys.fileRead("C:\\R4OS\\CONFIG\\ASSOC.R4S", &bytes);
    if (got > 0) {
        if (got >= bytes.len) return error.InvalidAssociations;
        _ = assoc.loadFromBytes(bytes[0..@intCast(got)]);
    }
    _ = r4std.subsystem_runtime.load(sys);
    var args: [r4os.subsystem_launch.max_args_bytes]u8 = undefined;
    var resolution: r4std.file_handler.Resolution = .{};
    var access: r4std.subsystem_runtime.AccessStats = .{};
    try r4std.file_handler.resolvePath(sys, &assoc, r4std.subsystem_runtime.catalog(), path, &args, &resolution, &access);
    if (resolution.state != .selected) return error.NoUniqueAssociation;
    const selected = resolution.target orelse return error.NoUniqueAssociation;
    if (selected.kind == .subsystem and !r4std.subsystem_runtime.hostPresent(sys, selected.app_path)) return error.MissingSubsystemHost;
    try w.copy(&message.path, selected.app_path);
    try w.copy(&message.args, selected.args);
    try w.copy(&message.title, selected.title);
    message.policy = @intFromEnum(selected.policy);
}
