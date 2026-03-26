add_rules("mode.debug", "mode.release")
set_kind("binary")
add_files("*.cc")
add_defines("LOCAL_FREOPEN")
set_targetdir("bin")
set_languages("c++20")

if is_mode("debug") then
    set_symbols("debug")
elseif is_mode("release") then
    set_optimize("faster")
    set_symbols("hidden")
    set_strip("all")
end

add_requires("imgui v1.92.3", {configs = {glfw = true, opengl3 = true}})
add_packages("imgui")

target("ai-caro")
    on_config(function (target)
        if target:toolchain("msvc") then
            target:add("cxflags", "/W4", "/permissive-")
            target:add("cxflags", "/GL")
            target:add("ldflags", "/LTCG")
            if is_mode("release") then
                target:set("runtimes", "MT")
            else
                target:set("runtimes", "MDd")
            end
        
        -- target clang on windows this will be treated as windows-msvc target, prefer llvm instead
        elseif target:toolchain("gcc") or target:toolchain("clang") or target:toolchain("llvm") then    
            target:add("cxflags", 
                "-Werror", "-Wall", "-Wextra", "-pedantic", "-Wshadow", 
                "-Wformat=2", "-Wfloat-equal", "-Wconversion", 
                "-Wcast-qual", "-Wcast-align", "-march=nehalem"
            )

            if is_mode("release") then
                target:add("ldflags", "-static-libstdc++", "-static-libgcc")
                if target:toolchain("gcc") then
                    target:add("ldflags", "-flto")
                end
    
                if is_plat("windows") then
                    target:add("ldflags", "-static")
                end
            end
        else 
            raise("Unsupported toolchain: " .. (target:get("toolchain") or "unknown"))
        end
    end)

-- Dev target: release build without static linking
target("ai-caro-dev")
    add_cxxflags("-Wall", "-Wextra", "-pedantic", "-Wshadow", 
        "-Wformat=2", "-Wfloat-equal", "-Wconversion", 
        "-Wcast-qual", "-Wcast-align", "-march=native"
    )
    add_ldflags("-fsanitize=address", "-fsanitize=undefined")