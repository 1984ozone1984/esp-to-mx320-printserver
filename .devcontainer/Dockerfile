# One container for the project: the pinned ESP-IDF toolchain, the bench tier
# (pytest + TestbenchDriver) and the self-hosted GitHub Actions runner.
FROM espressif/idf:v6.0.2

ARG HARNESS_REF=db6a093
ARG RUNNER_VERSION=2.321.0

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake build-essential curl jq git sudo libicu-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash dev && echo 'dev ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/dev

# TestbenchDriver from the harness repo, pinned to a commit.
RUN git clone https://github.com/SensorsIot/Embedded-AI-Harness.git /opt/harness \
    && git -C /opt/harness checkout ${HARNESS_REF}
ENV PYTHONPATH=/opt/harness/pytest

RUN python3 -m pip install --no-cache-dir 'pytest==8.*' 'pyserial==3.5'

# GitHub Actions runner: installed here, registered only by the owner
# (.devcontainer/runner/register.sh).
RUN mkdir -p /opt/actions-runner && cd /opt/actions-runner \
    && curl -fsSL -o runner.tgz \
       https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz \
    && tar xzf runner.tgz && rm runner.tgz \
    && ./bin/installdependencies.sh \
    && chown -R dev:dev /opt/actions-runner

USER dev
RUN echo '. $IDF_PATH/export.sh >/dev/null 2>&1' >> /home/dev/.bashrc
