add_rules("mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina")
add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("BedrockServerClientInterface")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines(
        "_HAS_CXX23=1",
        "NOMINMAX",
        "UNICODE",
        "BSCI_EXPORTS"
    )
    add_files(
        "src/**.cpp"
    )
    add_headerfiles("src/(bsci/**.h)")
    add_includedirs(
        "src"
    )
    add_packages("levilamina")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
