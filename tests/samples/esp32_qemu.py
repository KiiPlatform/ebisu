#!/usr/bin/env python3
"""Runs the ESP32 sample in Espressif's QEMU, and checks what it does.

  esp32_qemu.py refused <flash image>
      Built with its clock's floor after Kii Cloud's certificate expires, the
      sample has to refuse the certificate as expired, and so never onboard.

  esp32_qemu.py live-config <sdkconfig fragment> <run file>
      Writes the settings of a build for the live run: the application's, from
      APP_ID and DEFAULT_SITE, and a thing of the run's own. The run file keeps
      the thing's for the live run.

  esp32_qemu.py live <flash image> <run file>
      Against the live application, as its admin, with CLIENT_ID and
      CLIENT_SECRET. A user owns the run's thing. The sample onboards it and
      uploads its state, which the user reads back, and receives a command the
      user sends, whose results the user reads back. The user and the thing are
      deleted at the end, and the admin's token revoked.

The flash image is the sample's build merged into one, as esptool's merge_bin
writes it. The device's console is printed as it comes.

CI logs are public: tokens and passwords are never printed, and a failed
request is described by its status and errorCode only. The admin's and the
user's tokens are requested to expire within minutes, as the large tests' are.
"""

import json
import os
import queue
import re
import secrets
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request

THING_TYPE = "ltest_esp32_type"
FIRMWARE_VERSION = "ltest_esp32_fw"
TRAIT = "ltest_esp32_trait"
ALIAS = "ltest_esp32_alias"
TOKEN_LIFETIME_MS = 20 * 60 * 1000

# What the device's console shows when it has crashed.
CRASH = re.compile(r"Guru Meditation|abort\(\) was called|Backtrace:|stack overflow|assert failed")
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def fail(message):
    print(f"esp32_qemu: FAILED: {message}", flush=True)
    sys.exit(1)


def step(message):
    print(f"esp32_qemu: {message}", flush=True)


class Device:
    """The sample running in QEMU, with its console's lines as they come."""

    def __init__(self, flash):
        self.proc = subprocess.Popen(
            ["qemu-system-xtensa", "-nographic", "-machine", "esp32", "-m", "4M",
             "-drive", f"file={flash},if=mtd,format=raw",
             "-nic", "user,model=open_eth"],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.lines = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for raw in self.proc.stdout:
            line = ANSI.sub("", raw.decode("utf-8", "replace")).rstrip()
            print(f"device| {line}", flush=True)
            self.lines.put(line)
        self.lines.put(None)

    def wait_for(self, pattern, seconds, refuse=None):
        """The first line matching pattern, within seconds. Fails on a crash,
        on a line matching refuse, or if QEMU exits."""
        deadline = time.monotonic() + seconds
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                fail(f"no line matching '{pattern}' within {seconds} seconds")
            try:
                line = self.lines.get(timeout=left)
            except queue.Empty:
                continue
            if line is None:
                fail("QEMU exited")
            if CRASH.search(line):
                fail(f"the device crashed: {line}")
            if refuse is not None and re.search(refuse, line):
                fail(f"unexpected: {line}")
            if re.search(pattern, line):
                return line

    def stop(self):
        self.proc.kill()
        self.proc.wait()


def refused(flash):
    device = Device(flash)
    try:
        line = device.wait_for(r"server certificate for \S+ refused: expired", 180,
                refuse=r"onboarded|author updated")
        step(f"refused as it should be: {line}")
    finally:
        device.stop()


class Response:
    def __init__(self, status, body):
        self.status = status
        self.body = body

    def ok(self):
        return 200 <= self.status < 300

    def get(self, field):
        if isinstance(self.body, dict) and isinstance(self.body.get(field), str):
            return self.body[field]
        return ""

    def describe(self):
        d = f"status {self.status}"
        if self.get("errorCode"):
            d += f", errorCode {self.get('errorCode')}"
        return d


class Kii:
    def __init__(self):
        self.app_id = os.environ["APP_ID"]
        self.site = os.environ["DEFAULT_SITE"]

    def call(self, method, path, content_type=None, body=None, token=None):
        """A request, a 429 retried. body is JSON unless a str."""
        headers = {"X-Kii-AppID": self.app_id, "X-Kii-AppKey": "dummy"}
        data = None
        if body is not None:
            data = (body if isinstance(body, str) else json.dumps(body)).encode()
        if content_type is not None:
            headers["Content-Type"] = content_type
        if token is not None:
            headers["Authorization"] = f"Bearer {token}"
        for attempt in range(5):
            time.sleep(attempt)
            request = urllib.request.Request(f"https://{self.site}{path}", data=data,
                    headers=headers, method=method)
            try:
                with urllib.request.urlopen(request, timeout=30) as r:
                    status, raw = r.status, r.read()
            except urllib.error.HTTPError as e:
                status, raw = e.code, e.read()
            if status != 429:
                break
        try:
            parsed = json.loads(raw) if raw else None
        except ValueError:
            parsed = None
        return Response(status, parsed)

    def app(self, rest):
        return f"/api/apps/{self.app_id}{rest}"

    def thing_if(self, rest):
        return f"/thing-if/apps/{self.app_id}{rest}"


def now_ms():
    return int(time.time() * 1000)


def require(response, what, also=()):
    if not (response.ok() or response.status in also):
        fail(f"{what}: {response.describe()}")
    return response


def poll(seconds, call, done):
    deadline = time.monotonic() + seconds
    while True:
        r = call()
        if done(r) or time.monotonic() > deadline:
            return r
        time.sleep(3)


def live_config(fragment, run_file):
    run = str(time.time_ns() // 1000)
    thing = {
        "vendor_thing_id": f"ltest_esp32_{run}",
        "password": secrets.token_hex(12),
        "run": run,
    }
    state = json.dumps({ALIAS: {"mode": run}}, separators=(",", ":"))

    def quoted(s):
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

    with open(fragment, "w") as f:
        f.write(f"CONFIG_EBISU_SAMPLE_APP_ID={quoted(os.environ['APP_ID'])}\n")
        f.write(f"CONFIG_EBISU_SAMPLE_APP_HOST={quoted(os.environ['DEFAULT_SITE'])}\n")
        f.write(f"CONFIG_EBISU_SAMPLE_VENDOR_THING_ID={quoted(thing['vendor_thing_id'])}\n")
        f.write(f"CONFIG_EBISU_SAMPLE_THING_PASSWORD={quoted(thing['password'])}\n")
        f.write(f"CONFIG_EBISU_SAMPLE_THING_TYPE={quoted(THING_TYPE)}\n")
        f.write(f"CONFIG_EBISU_SAMPLE_FIRMWARE_VERSION={quoted(FIRMWARE_VERSION)}\n")
        f.write(f"CONFIG_EBISU_SAMPLE_STATE={quoted(state)}\n")
        f.write("CONFIG_EBISU_SAMPLE_UPDATE_PERIOD=10\n")
    with open(run_file, "w") as f:
        json.dump(thing, f)
    step(f"run {run}")


def set_up_application(kii, admin):
    """The thing type, its firmware version, the trait and its alias, unless
    they are there already."""
    thing_type = kii.app(f"/configuration/thing-types/{THING_TYPE}")
    require(kii.call("PUT", thing_type, "application/vnd.kii.ThingTypeConfigurationRequest+json",
            {"simpleFlow": True, "verificationCodeFlowStartedByUser": True,
             "verificationCodeFlowStartedByThing": True, "verificationCodeTimeout": 172800,
             "verificationCodeLength": 9}, admin), "thing type")
    require(kii.call("PUT", f"{thing_type}/firmware-versions/{FIRMWARE_VERSION}", "text/plain", "",
            admin), "firmware version", also=(409,))

    trait = kii.thing_if(f"/traits/{TRAIT}/versions")
    r = kii.call("GET", f"{trait}/1", token=admin)
    if r.status == 404:
        def schema(name):
            return {name: {"description": "", "payloadSchema": {"type": "string"}}}
        require(kii.call("POST", trait, "application/vnd.kii.TraitCreationRequest+json",
                {"dataGroupingInterval": "15_MINUTES", "actions": [schema("setMode")],
                 "states": [schema("mode")]}, admin), "trait creation")
    else:
        require(r, "trait")

    require(kii.call("PUT", kii.thing_if(f"/configuration/thing-types/{THING_TYPE}"
            f"/firmware-versions/{FIRMWARE_VERSION}/aliases/{ALIAS}"),
            "application/vnd.kii.TraitAliasCreationRequest+json",
            {"trait": TRAIT, "traitVersion": 1}, admin), "alias", also=(409,))


def live(flash, run_file):
    with open(run_file) as f:
        thing = json.load(f)
    run = thing["run"]
    kii = Kii()

    r = require(kii.call("POST", "/api/oauth2/token", "application/json",
            {"client_id": os.environ["CLIENT_ID"], "client_secret": os.environ["CLIENT_SECRET"],
             "expiresAt": now_ms() + TOKEN_LIFETIME_MS}), "admin token")
    admin = r.get("access_token")
    if not admin:
        fail("admin token: none in the response")
    to_delete = []
    device = None
    try:
        set_up_application(kii, admin)
        step("application set up")

        login = f"ltest_esp32_{run}"
        user_password = secrets.token_hex(12)
        r = require(kii.call("POST", kii.app("/users"), "application/vnd.kii.RegistrationRequest+json",
                {"loginName": login, "password": user_password}, admin), "user registration")
        user_id = r.get("userID")
        to_delete.append(kii.app(f"/users/{user_id}"))
        r = require(kii.call("POST", kii.app("/oauth2/token"), "application/json",
                {"grant_type": "password", "username": login, "password": user_password,
                 "expiresAt": now_ms() + TOKEN_LIFETIME_MS}), "user token")
        user = r.get("access_token")

        # The user onboards the thing, as an application does, and so owns it.
        r = require(kii.call("POST", kii.thing_if("/onboardings"),
                "application/vnd.kii.OnboardingWithVendorThingIDByOwner+json",
                {"vendorThingID": thing["vendor_thing_id"], "thingPassword": thing["password"],
                 "owner": f"user:{user_id}", "thingType": THING_TYPE,
                 "firmwareVersion": FIRMWARE_VERSION}, user), "onboarding by the owner")
        thing_id = r.get("thingID")
        to_delete.append(kii.app(f"/things/{thing_id}"))
        target = kii.thing_if(f"/targets/thing:{thing_id}")
        step("thing onboarded by its owner")

        # The device onboards with the same vendor thing ID and password: its
        # first connection is checked against the clock's floor, and the
        # response gives the clock the time, which the next are checked at.
        device = Device(flash)
        device.wait_for(r"\bonboarded\b", 240, refuse=r"refused")
        device.wait_for(r"the time: \d+, exact", 10)
        step("device onboarded, its clock exact")
        device.wait_for(r"handler ready", 240, refuse=r"refused")
        step("handler ready")

        r = poll(180, lambda: kii.call("GET", f"{target}/states", token=user),
                lambda r: r.ok() and isinstance(r.body, dict)
                and isinstance(r.body.get(ALIAS), dict) and r.body[ALIAS].get("mode") == run)
        if not (r.ok() and r.body.get(ALIAS, {}).get("mode") == run):
            fail(f"the device's state did not arrive: {r.describe()}")
        step("state read back")

        mode = f"command-{run}"
        r = require(kii.call("POST", f"{target}/commands", "application/vnd.kii.CommandCreationRequest+json",
                {"issuer": f"user:{user_id}", "actions": [{ALIAS: [{"setMode": mode}]}]}, user),
                "command")
        command_id = r.get("commandID")
        device.wait_for(rf"action {ALIAS} setMode: {mode}", 120)
        r = poll(120, lambda: kii.call("GET", f"{target}/commands/{command_id}", token=user),
                lambda r: r.get("commandState") in ("DONE", "INCOMPLETE"))
        if r.get("commandState") != "DONE":
            fail(f"the command did not complete: {r.describe()}, state '{r.get('commandState')}'")
        step("command done")
    finally:
        if device is not None:
            device.stop()
        for path in reversed(to_delete):
            r = kii.call("DELETE", path, token=admin)
            if not r.ok():
                step(f"cleaning up: {r.describe()}")
        kii.call("POST", kii.app("/logout"), "application/vnd.kii.LogoutRequest+json",
                {"type": "SPECIFIC_TOKEN", "token": admin}, admin)


def main(argv):
    if len(argv) == 3 and argv[1] == "refused":
        refused(argv[2])
    elif len(argv) == 4 and argv[1] == "live-config":
        live_config(argv[2], argv[3])
    elif len(argv) == 4 and argv[1] == "live":
        live(argv[2], argv[3])
    else:
        print(__doc__)
        sys.exit(2)
    step("passed")


if __name__ == "__main__":
    main(sys.argv)
