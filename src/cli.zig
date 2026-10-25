const std = @import("std");
const r4os = @import("r4os");
const w = @import("desktop_launch");
pub const Options = struct {
    target: []const u8 = "",
    args: []const u8 = "",
    open: bool = false,
    wait_ms: u64 = 5000,
    frame: bool = false,
    json: bool = false,
    help: bool = false,
    key: ?w.Message = null,
};
fn eq(a: []const u8, b: []const u8) bool {
    return std.ascii.eqlIgnoreCase(a, b);
}
pub fn parse(raw: []const u8) !Options {
    if (!std.unicode.utf8ValidateSlice(raw) or std.mem.indexOfScalar(u8, raw, 0) != null) return error.InvalidText;
    var result = Options{};
    var rest = std.mem.trimStart(u8, raw, " \t");
    while (rest.len != 0) {
        var token: []const u8 = undefined;
        const quoted = rest[0] == '"';
        if (quoted) {
            const end = std.mem.indexOfScalarPos(u8, rest, 1, '"') orelse return error.UnclosedQuote;
            token = rest[1..end];
            rest = rest[end + 1 ..];
            if (rest.len != 0 and rest[0] != ' ' and rest[0] != '\t') return error.InvalidQuote;
        } else {
            const end = std.mem.indexOfAny(u8, rest, " \t") orelse rest.len;
            token = rest[0..end];
            rest = rest[end..];
        }
        rest = std.mem.trimStart(u8, rest, " \t");
        if (!quoted and eq(token, "/JSON")) {
            result.json = true;
            continue;
        }
        if (!quoted and (eq(token, "/?") or eq(token, "/HELP"))) {
            result.help = true;
            continue;
        }
        if (!quoted and eq(token, "/OPEN")) {
            result.open = true;
            continue;
        }
        if (!quoted and eq(token, "/WAITFRAME")) {
            result.frame = true;
            continue;
        }
        if (!quoted and token.len > 6 and eq(token[0..6], "/WAIT:")) {
            result.wait_ms = std.fmt.parseInt(u64, token[6..], 10) catch return error.InvalidWait;
            if (result.wait_ms == 0 or result.wait_ms > 300000) return error.InvalidWait;
            continue;
        }
        if (!quoted and token.len > 8 and eq(token[0..8], "/STATUS:")) {
            result.key = try parseKey(token[8..]);
            if (rest.len != 0 or result.open or result.frame) return error.InvalidStatus;
            return result;
        }
        if (token.len == 0 or (!quoted and token[0] == '/' and std.mem.indexOfScalar(u8, token[1..], '/') == null)) return error.InvalidTarget;
        result.target = token;
        // Everything after the target belongs to the application, including
        // quotes, repeated/trailing spaces and switches such as /JSON.
        result.args = rest;
        if (result.args.len >= 128 or (result.open and result.args.len != 0)) return error.InvalidArguments;
        return result;
    }
    if (!result.help) return error.MissingTarget;
    return result;
}
pub fn parseKey(raw: []const u8) !w.Message {
    var it = std.mem.splitScalar(u8, raw, ':');
    var values: [4]u64 = undefined;
    for (&values) |*value| {
        value.* = std.fmt.parseInt(u64, it.next() orelse return error.InvalidKey, 10) catch return error.InvalidKey;
        if (value.* == 0) return error.InvalidKey;
    }
    if (it.next() != null or values[0] > std.math.maxInt(u32) or values[2] > std.math.maxInt(u32)) return error.InvalidKey;
    return .{ .broker = .{ .instance = @intCast(values[0]), .generation = values[1] }, .request = .{ .instance = @intCast(values[2]), .generation = values[3] } };
}
pub fn absolute(target: []const u8, cwd: []const u8) !r4os.path.AbsoluteFilePath {
    if (target.len >= 3 and target[1] == ':' and (target[2] == '\\' or target[2] == '/')) return r4os.path.AbsoluteFilePath.parse(target);
    // Drive-relative X:foo is ambiguous without per-drive working directories.
    if (target.len >= 2 and target[1] == ':') return error.ExpectedAbsolute;
    const base = try r4os.path.AbsoluteFilePath.parse(cwd);
    var bytes: [r4os.path.file_path_max + 1]u8 = undefined;
    const prefix = if (target.len != 0 and (target[0] == '\\' or target[0] == '/')) base.bytes()[0..2] else base.bytes();
    const separator = if (prefix.len == 2 or prefix[prefix.len - 1] == '\\') "" else "\\";
    const path = std.fmt.bufPrint(&bytes, "{s}{s}{s}", .{ prefix, separator, target }) catch return error.TooLong;
    return r4os.path.AbsoluteFilePath.parse(path);
}
test "literal application argument tail and UTF-8 paths" {
    const value = try parse("/JSON /WAIT:3000 /WAITFRAME \"C:\\Grüße Welt\\A.R4X\" \"a b\"  /JSON  ");
    try std.testing.expectEqualStrings("C:\\Grüße Welt\\A.R4X", value.target);
    try std.testing.expectEqualStrings("\"a b\"  /JSON  ", value.args);
    try std.testing.expect(value.json and value.frame and value.wait_ms == 3000);
    try std.testing.expectError(error.UnclosedQuote, parse("\"C:\\broken"));
    try std.testing.expectError(error.InvalidArguments, parse("/OPEN C:\\folder extra"));
    try std.testing.expectError(error.InvalidWait, parse("/WAIT:0 C:\\app.R4X"));
}
test "relative paths bind to caller working directory and keys retain both generations" {
    const path = try absolute("..\\SNES\\Test Welt.sfc", "D:\\Roms\\GB");
    try std.testing.expectEqualStrings("D:\\Roms\\SNES\\Test Welt.sfc", path.bytes());
    try std.testing.expectError(error.ExpectedAbsolute, absolute("D:Game.sfc", "C:\\"));
    const key = (try parse("/JSON /STATUS:4:600:7:900")).key.?;
    try std.testing.expectEqual(@as(u64, 600), key.broker.generation);
    try std.testing.expectEqual(@as(u64, 900), key.request.generation);
    try std.testing.expectError(error.InvalidKey, parseKey("4:600:7:0"));
}
