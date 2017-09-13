set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
AWS_FPGA_REPO_DIR=${AWS_FPGA_REPO_DIR}
PYRILOG_REPO_DIR=${PYRILOG_REPO_DIR} || "/home/centos/pyrilog"

if [ ! -d "$AWS_FPGA_REPO_DIR" ]; then
	echo "AWS_FPGA_REPO_DIR not found."
	cd /home/centos/src/project_data
	git clone https://github.com/aws/aws-fpga.git
fi

# AWS FPGA HDK and SDK Setup
echo "AWS_FPGA_REPO_DIR found at $AWS_FPGA_REPO_DIR. Setting up HDK and SDK."
cd $AWS_FPGA_REPO_DIR

source ./hdk_setup.sh
echo "HDK setup complete"

source ./sdk_setup.sh
echo "SDK setup success."

# Setup EDMA
echo "Installing AWS Linux EDMA kernel..."
cd $AWS_FPGA_REPO_DIR/sdk/linux_kernel_drivers/edma
make
echo "EDMA installation success!"

# Install Xtst libs for Vivado GUI X11 forwarding
echo "Installing dependencies for Vivado GUI X11 forwarding...
sudo yum install -y libXtst-devel libXtst"
echo "X11 dependencies installed."

# Install XAuth for X11 support
echo "Installing XAuth dependencies for X11 support"
sudo yum install -y xorg-x11-server-Xorg xorg-x11-xauth xorg-x11-apps
echo "Xauth dependencies installed."

# Allow for Display offset
echo "Update sshd_config"
sudo sed -i '/#X11DisplayOffset 10/c\X11DisplayOffset 10' /etc/ssh/sshd_config
echo "/etc/ssh/sshd_config updated."

# Setup IPI in Vivado
echo "Setting up Tcl init scripts"
cd /home/centos/.Xilinx/Vivado
if [ ! -f "init.tcl" ]; then
	touch Vivado_init.tcl
fi
echo "source $HDK_SHELL_DIR/hlx/hlx_setup.tcl" > Vivado_init.tcl

# Install Python for Pyrilog
sudo yum -y install yum-utils
sudo yum -y groupinstall development
# sudo yum -y install https://centos7.iuscommunity.org/ius-release.rpm
sudo yum -y install python36u python-pip

# Setup Python `virtualenv` dependencies
sudo pip install -U pip
sudo pip install -U virtualenv
if [ ! -d "/home/centos/pyenvs" ]; then
	mkdir /home/centos/pyenvs
fi

# Clone the Pyrilog repository
if [ ! -d "$PYRILOG_REPO_DIR" ]; then
	echo "PYRILOG_REPO_DIR not found."
	cd /home/centos
	git clone git@github.com:xbili/pyrilog.git
	export PYRILOG_REPO_DIR="/home/centos/pyrilog"
	echo "Cloned Pyrilog git repository."
fi

# Setup new `virtualenv`
cd /home/centos/pyenvs
virtualenv -p python3.6 pyrilog

# Enter virtualenv
source pyrilog/bin/activate

# Install dependencies in virtualenv
cd /home/centos/pyrilog
pip install -r requirements.txt
deactivate # Leave the Python environment

# Finish!
cd $DIR
echo Success! Log out and log in again for X11 support.

set +e
