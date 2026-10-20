FROM ghcr.io/ptero-eggs/yolks@sha256:db48c30fc50104a43d951200174d33e2b9200f80b03b3ded9c106586450dbcf8

ARG SOURCE_REPOSITORY=https://github.com/Draakoor/renegade-campaign-coop
ARG VCS_REF=unknown
LABEL org.opencontainers.image.title="Renegade Campaign Coop development runtime" \
      org.opencontainers.image.description="Wine runtime with M06/M07/M09 coop adaptations. Human campaign playtests pending." \
      org.opencontainers.image.source=$SOURCE_REPOSITORY \
      org.opencontainers.image.revision=$VCS_REF \
      org.opencontainers.image.licenses="GPL-2.0-or-later AND GPL-3.0-or-later"

USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
      perl libpoe-perl libxml-simple-perl libxml-parser-perl libdbi-perl \
      libdbd-sqlite3-perl libfile-homedir-perl libclass-unload-perl \
    && rm -rf /var/lib/apt/lists/*
RUN groupadd --gid 1000 container \
    && useradd --uid 1000 --gid container --home-dir /home/container --shell /bin/bash --no-create-home container \
    && mkdir -p /home/container \
    && chown container:container /home/container
WORKDIR /opt/renegade-coop
COPY bootstrap.py runtime.py brenbot_runtime.py gamespy.py volume_lock.py sources.lock.json COPYING COPYING.plugin THIRD_PARTY.txt ./
COPY brenbot/ brenbot/
COPY defaults/ defaults/
COPY tools/*.py tools/
COPY plugin/ plugin/
COPY artifacts/da_campaign.dll artifacts/da_campaign.dll
COPY docker/entrypoint.sh /usr/local/bin/renegade-entrypoint
RUN chmod 755 /usr/local/bin/renegade-entrypoint \
    && python3 tools/build_brenbot.py --output /opt/brenbot \
    && python3 -m compileall -q /opt/renegade-coop \
    && test -s artifacts/da_campaign.dll

ENV AUTO_UPDATE=0 RELEASE_MODE=development RENEGADE_ROOT=/home/container \
    WINEPREFIX=/home/container/.wine WINEDEBUG=-all PYTHONUNBUFFERED=1
USER container
WORKDIR /home/container
EXPOSE 35000/udp 35001/udp 35002/tcp
STOPSIGNAL SIGTERM
ENTRYPOINT ["/usr/local/bin/renegade-entrypoint"]
CMD []
