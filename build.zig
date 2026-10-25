const std = @import("std");
pub fn build(b: *std.Build) void {
    const sdk_build = b.lazyImport(@This(), "r4os_sdk") orelse return;
    const sdk_dep = b.dependencyFromBuildZig(sdk_build, .{});
    const lib_build = b.lazyImport(@This(), "r4os_libraries") orelse return;
    const lib_dep = b.dependencyFromBuildZig(lib_build, .{});
    const sdk = sdk_build.sdk(b, sdk_dep, .{});
    const wire_path: std.Build.LazyPath = .{ .cwd_relative = b.pathResolve(&.{"../../Services/WindowService/Contract/desktop_launch_v1.zig"}) };
    _ = sdk.addR4MFWithOptions(b.path("module.R4MF"), .{ .zig_module_roots = &.{ lib_dep.namedLazyPath("r4std_zig_binding"), wire_path } });
    _ = sdk.addR4MF(b.path("Tests/Guest/Control/module.R4MF"));
    _ = sdk.addR4MF(b.path("Tests/Guest/Gui/module.R4MF"));
    const host = sdk.createR4osModule(b.graph.host, .Debug);
    const wire = b.createModule(.{ .root_source_file = wire_path, .target = b.graph.host });
    const root = b.createModule(.{ .root_source_file = b.path("src/cli.zig"), .target = b.graph.host });
    root.addImport("r4os", host);
    root.addImport("desktop_launch", wire);
    b.step("test", "Argument preservation and request-key parsing").dependOn(&b.addRunArtifact(b.addTest(.{ .root_module = root })).step);
}
