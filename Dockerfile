FROM ghcr.io/ptero-eggs/yolks@sha256:db48c30fc50104a43d951200174d33e2b9200f80b03b3ded9c106586450dbcf8

ARG SOURCE_REPOSITORY=https://github.com/Draakoor/renegade-campaign-coop
ARG VCS_REF=unknown
LABEL org.opencontainers.image.title="Renegade Campaign Coop development runtime" \
      org.opencontainers.image.description="Wine runtime, campaign plugin and asset installer. M06/M07/M09 and campaign acceptance remain incomplete." \
      org.opencontainers.image.source=$SOURCE_REPOSITORY \
      org.opencontainers.image.revision=$VCS_REF \
      org.opencontainers.image.licenses="GPL-2.0-or-later"

USER root
WORKDIR /opt/renegade-coop
COPY bootstrap.py runtime.py volume_lock.py sources.lock.json COPYING THIRD_PARTY.txt ./
COPY defaults/ defaults/
COPY tools/*.py tools/
COPY plugin/ plugin/
COPY artifacts/da_campaign.dll artifacts/da_campaign.dll
COPY docker/entrypoint.sh /usr/local/bin/renegade-entrypoint
RUN chmod 755 /usr/local/bin/renegade-entrypoint \
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
