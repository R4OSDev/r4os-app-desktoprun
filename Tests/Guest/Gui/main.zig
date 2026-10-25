//! Visible argument oracle: records exactly the received tail, independent of
//! DESKRUN parsing/transport. Uses ordinary application window lifetime.
const std = @import("std");
const r4os = @import("r4os");
pub fn r4_app_main(app: *r4os.App) i32 {
    const sys = app.system();
    var path: [96]u8 = undefined;
    const name = std.fmt.bufPrintZ(&path, "C:\\TEMP\\DRARGS-{d}.TXT", .{app.raw.instance_id}) catch return 1;
    if (sys.fileWrite(name, app.args()) != app.args().len) return 2;
    var timers: [1]r4os.Timer = .{.{}};
    var window = app.window(&timers) orelse return 3;
    _ = window.setTitle("DESKRUN argument oracle");
    const silent = std.mem.eql(u8, app.args(), "NOFRAME");
    if (!silent) paint(&window, app.args());
    while (!sys.programShouldClose()) {
        switch (window.waitMessage(r4os.time_contract.timeoutForever())) {
            .message => |message| switch (message) {
                .close => return 0,
                .resize => if (!silent) {
                    paint(&window, app.args());
                },
                else => {},
            },
            .failure => return 4,
            else => {},
        }
    }
    return 0;
}
fn paint(window: *r4os.Window, args: []const u8) void {
    var frame = switch (window.beginPaint()) {
        .paint => |v| v,
        .failure => return,
    };
    defer frame.discard();
    _ = frame.canvas.clear(0xffffff);
    _ = frame.canvas.text(12, 12, "DESKRUN argument oracle", 0x000080, 0xffffff);
    var text: [128:0]u8 = .{0} ** 128;
    const n = @min(args.len, text.len - 1);
    @memcpy(text[0..n], args[0..n]);
    _ = frame.canvas.text(12, 36, &text, 0, 0xffffff);
    _ = frame.present();
}
