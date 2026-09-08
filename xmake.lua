add_rules("mode.release", "mode.debug")

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina 26.32.2", {configs = {target_type = get_config("target_type")}})
add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("BedrockServerClientInterface")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_defines( "_HAS_CXX23=1", "NOMINMAX", "UNICODE", "BSCI_EXPORTS")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    -- add_defines("TEST")
    add_files("src/**.cpp")
    add_headerfiles("src/(bsci/**.h)")
    add_includedirs("src")
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    if is_config("target_type", "server") then
    --  add_includedirs("src-server")
    --  add_files("src-server/**.cpp")
    else
    --  add_includedirs("src-client")
    --  add_files("src-client/**.cpp")
    end
    after_buildcmd(function(target, batchcmds)
    local outputdir = path.join(os.projectdir(), "bin", target:name())
    local assetsdir = path.join(os.projectdir(), "assets")
        if os.isdir(assetsdir) then
            batchcmds:mkdir(outputdir)
            batchcmds:cp(path.join(assetsdir, "*"), outputdir)
        end
    end)
