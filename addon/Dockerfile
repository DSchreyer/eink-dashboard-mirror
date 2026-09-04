ARG BUILD_FROM
FROM $BUILD_FROM

# No ESPHome/PlatformIO toolchain needed here -- this add-on only
# screenshots a URL and writes a file; the ESP32 side pulls it over WiFi
# on its own, so there's nothing to compile or push.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        chromium \
        fonts-dejavu-core \
        fonts-liberation \
        python3 \
        python3-pip \
        python3-venv \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
# playwright's Python package only -- NOT `playwright install chromium`,
# since screenshot.py points it at the apt-installed /usr/bin/chromium
# above instead of downloading a second copy of the browser. flask serves
# the config editor's ingress panel + API; requests talks to the
# Supervisor API to persist saved options.
RUN pip install --no-cache-dir pillow playwright flask requests

COPY app/ /app/
COPY rootfs/ /

RUN chmod a+x /etc/services.d/eink-dashboard-mirror/run
