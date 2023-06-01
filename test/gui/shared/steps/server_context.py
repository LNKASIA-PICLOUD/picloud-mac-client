import urllib.request
import json
from os import path
from helpers.ConfigHelper import get_config
from helpers.api.Provisioning import setup_app
import helpers.api.webdav_helper as webdav
import helpers.api.sharing_helper as sharing_helper


def executeStepThroughMiddleware(context, step):
    body = {"step": step}
    if hasattr(context, "table"):
        body["table"] = context.table

    params = json.dumps(body).encode('utf8')

    req = urllib.request.Request(
        path.join(get_config('middlewareUrl'), 'execute'),
        data=params,
        headers={"Content-Type": "application/json"},
        method='POST',
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )


@Given(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Given " + stepPart1 + " " + stepPart2)
    global usersDataFromMiddleware
    usersDataFromMiddleware = None


@Given(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Given " + stepPart1)
    global usersDataFromMiddleware
    usersDataFromMiddleware = None


@Then(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Then " + stepPart1 + " " + stepPart2)


@Then(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Then " + stepPart1)


@Given('app "|any|" has been "|any|" in the server')
def step(context, app_name, action):
    setup_app(app_name, action)


@Then(
    r'^as "([^"].*)" (?:file|folder) "([^"].*)" should not exist in the server',
    regexp=True,
)
def step(context, user_name, resource_name):
    test.compare(
        webdav.resource_exists(user_name, resource_name),
        False,
        f"Resource '{resource_name}' should not exist, but does",
    )


@Then(
    r'^as "([^"].*)" (?:file|folder) "([^"].*)" should exist in the server', regexp=True
)
def step(context, user_name, resource_name):
    test.compare(
        webdav.resource_exists(user_name, resource_name),
        True,
        f"Resource '{resource_name}' should exist, but does not",
    )


@Then('as "|any|" the file "|any|" should have the content "|any|" in the server')
def step(context, user_name, file_name, content):
    text_content = webdav.get_file_content(user_name, file_name)
    test.compare(
        text_content,
        content,
        f"File '{file_name}' should have content '{content}' but found '{text_content}'",
    )


@Then(
    r'as user "([^"].*)" the (?:file|folder) "([^"].*)" should have a public link in the server',
    regexp=True,
)
def step(context, user_name, resource_name):
    has_link_share = sharing_helper.has_public_link_share(user_name, resource_name)
    test.compare(
        has_link_share,
        True,
        f"Resource '{resource_name}' does not have public link share",
    )


@Then(
    r'as user "([^"].*)" the (?:file|folder) "([^"].*)" should not have any public link in the server',
    regexp=True,
)
def step(context, user_name, resource_name):
    has_link_share = sharing_helper.has_public_link_share(user_name, resource_name)
    test.compare(
        has_link_share, False, f"Resource '{resource_name}' have public link share"
    )
