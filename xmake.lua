set_languages("c++20")

set_warnings("allextra", "error")

set_optimize("fastest")

add_requires("libcurl >= 7")

target("url")
    set_kind("static")
    add_packages("libcurl")
    add_includedirs("include")
    add_files("src/url.cpp")

target("test")
    add_deps("url")
    add_includedirs("include")
    add_files("test/test.cpp")
