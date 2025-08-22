add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

if is_config("target_type", "server") then
    add_requires("levilamina", {configs = {target_type = "server"}})
else
    add_requires("levilamina", {configs = {target_type = "client"}})
end

add_requires("levibuildscript")
add_requires("tgbot-cpp")
add_requires("curl")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

target("TelegramBot")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina")
    add_packages("tgbot-cpp")
    add_packages("curl")
    set_exceptions("none") -- To avoid conflicts with /EHa.
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
