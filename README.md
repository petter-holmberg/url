# url
A simple C++20 module for making HTTP requests.

## Usage examples

    import url;

### HEAD

    // Check if this repository is online
    assert(url::get("github.com/petter-holmberg/url"));

### GET

    // Print contents of website
    std::cout << url::get("github.com");

    // Crawl with my custom user agent
    auto response = url::get(
        "https://example.com", {"User-Agent: Mybot/1.0 (+http www.mybot.com/bot.html)"}
    );
    assert(response.status_code == 200);

    // Print detailed information about the request
    auto response = url::get("github.com");
    std::cout << response.status_code << '\n';
    for (auto const& header : response.headers) std::cout << header << '\n';
    std::cout << response.body << '\n';
    std::cout << response.encoding << '\n';
    std::cout << response.url << '\n';

### POST

    // Post a login form
    auto response = url::post(
        "example.com/login",
        {
            {"username", "dopey"},
            {"password", "qwerty"}
        }
    );

    // Post JSON to a REST API
    auto response = url::post(
        "https://example.com/users",
        "{"
        "    \"username\": \"dopey\","
        "    \"password\": \"qwerty\""
        "}",
        {
            "Content-Type: application/json",
            "Accept: application/json"
        }
    );
    assert(response.status_code == 201);

### PUT

    // Put to a JSON REST API resource
    auto response = url::put(
        "https://example.com/users/dopey",
        "{"
        "    \"username\": \"dopey\","
        "    \"password\": \"qwerty123\""
        "}",
        {
            "Content-Type: application/json",
            "Accept: application/json"
        }
    );
    assert(response.status_code == 202);

### PATCH

    // Patch a JSON REST API resource
    auto response = url::put(
        "https://example.com/users/dopey",
        "{"
        "    \"oldpassword\": \"qwerty\""
        "    \"newpassword1\": \"qwerty123\""
        "    \"newpassword2\": \"qwerty123\""
        "}",
        {
            "Content-Type: application/json",
            "Accept: application/json"
        }
    );
    assert(response.status_code == 202);

### DELETE

    // Delete a REST API resource
    auto response = url::del("https://example.com/users/dopey"):
    assert(response.status_code == 204);
