#!/bin/bash

set -e
set -x

ENV_FILE=/tmp/env.sh

. ${ENV_FILE}

# setup MODULE_SRC_DIR env var
cwd=`pwd`
if [ -z "${MODULE_SRC_DIR}" ]; then
    if [ -e "$cwd/src/ssh2-module.cpp" ]; then
        MODULE_SRC_DIR=$cwd
    else
        MODULE_SRC_DIR=$WORKDIR/module-ssh2
    fi
fi
echo "export MODULE_SRC_DIR=${MODULE_SRC_DIR}" >> ${ENV_FILE}

echo "export QORE_UID=1000" >> ${ENV_FILE}
echo "export QORE_GID=1000" >> ${ENV_FILE}

. ${ENV_FILE}

export MAKE_JOBS=4

# build module and install
echo && echo "-- building module --"
mkdir -p ${MODULE_SRC_DIR}/build
cd ${MODULE_SRC_DIR}/build
cmake .. -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}
make install

# Verify that each module owns exactly its current provider presentation catalog contribution.
${MODULE_SRC_DIR}/test/docker_test/check-i18n.sh

# add Qore user and group
if ! grep -q "^qore:x:${QORE_GID}" /etc/group; then
    addgroup -g ${QORE_GID} qore
fi
if ! grep -q "^qore:x:${QORE_UID}" /etc/passwd; then
    adduser -u ${QORE_UID} -D -G qore -h /home/qore -s /bin/bash qore
fi

# generate SSH keys
gosu qore:qore ssh-keygen -q -f /home/qore/.ssh/id_rsa -N ""
gosu qore:qore cp /home/qore/.ssh/id_rsa.pub /home/qore/.ssh/authorized_keys
chmod 600 /home/qore/.ssh/authorized_keys

# turn on sshd debugging output
#echo LogLevel DEBUG3 >> /etc/ssh/sshd_config

# own everything by the qore user
chown -R qore:qore ${MODULE_SRC_DIR} /home/qore

passwd -u qore

# run SSH server
mkdir -p /var/run/sshd
/usr/sbin/sshd -h /home/qore/.ssh/id_rsa # -E /var/log/sshd

# run the tests
export QORE_MODULE_DIR=${MODULE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${MODULE_SRC_DIR}

for test in test/*.qtest; do
    gosu qore:qore qore $test -vv
done
