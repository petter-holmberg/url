/*
url - A simple C++20 library for making HTTP requests. Version 0.0.3.

Written in 2023 by Petter Holmberg petter.holmberg@usa.net

Uses libcurl: https://curl.se/libcurl/

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

It is also recommended that you include a file called COPYING (or COPYING.txt) containing the CC0 legalcode as plain text.
*/
#ifndef URL_HPP_V1
#define URL_HPP_V1

#include <concepts>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace url::inline v1 {

using header_t = std::vector<std::string>;

using form_t = std::vector<std::pair<std::string, std::string>>;

struct response_t
{
    uint32_t status_code{0};
    header_t headers;
    std::string body;
    std::string encoding;
    std::string url;

    [[nodiscard]] explicit constexpr
    operator bool() const noexcept
    {
        return status_code >= 100 && status_code < 400;
    }

    template <std::invocable<response_t&> F>
    constexpr auto
    and_then(std::invocable<response_t&> auto&& f) &
    {
        if (*this)
        {
            return std::invoke(std::forward<F>(f), *this);
        }
        else
        {
            return std::remove_cvref_t<std::invoke_result_t<F, response_t&>>();
        }
    }

    template <std::invocable<response_t const&> F>
    constexpr auto
    and_then(F&& f) const&
    {
        if (*this)
        {
            return std::invoke(std::forward<F>(f), *this);
        }
        else
        {
            return std::remove_cvref_t<std::invoke_result_t<F, response_t const&>>();
        }
    }

    template <std::invocable<response_t> F>
    constexpr auto
    and_then(F&& f) &&
    {
        if (*this)
        {
            return std::invoke(std::forward<F>(f), std::move(*this));
        }
        else
        {
            return std::remove_cvref_t<std::invoke_result_t<F, response_t>>();
        }
    }

    template <std::invocable<response_t const> F>
    constexpr auto
    and_then(F&& f) const&&
    {
        if (*this)
        {
            return std::invoke(std::forward<F>(f), std::move(*this));
        }
        else
        {
            return std::remove_cvref_t<std::invoke_result_t<F, response_t const>>();
        }
    }

    template <std::invocable F>
    constexpr auto
    or_else(F&& f) const&
    {
        if (*this)
        {
            return *this;
        }
        else
        {
            return std::invoke(std::forward<F>(f));
        }
    }

    template <std::invocable F>
    constexpr auto
    or_else(F&& f) &&
    {
        if (*this)
        {
            return std::move(*this);
        }
        else
        {
            return std::invoke(std::forward<F>(f));
        }
    }
};

auto operator<<(std::ostream& os, response_t const& response) -> std::ostream&;

void set_headers(header_t const& headers);

auto request() -> response_t;

auto update(std::string_view url, char const* method, std::string_view body, header_t const& headers) -> response_t;

auto del(std::string_view url, std::string_view body = {}, header_t const& headers = {}) -> response_t;

auto get(std::string_view url, header_t const& headers = {}) -> response_t;

auto head(std::string_view url, header_t const& headers = {}) -> response_t;

auto patch(std::string_view url, std::string_view body, header_t const& headers = {}) -> response_t;

auto post(std::string_view url, std::string_view body, header_t const& headers = {}) -> response_t;

auto post(std::string_view url, form_t form, header_t const& headers = {}) -> response_t;

auto put(std::string_view url, std::string_view body, header_t const& headers = {}) -> response_t;

}

#endif
