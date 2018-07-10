#include "../../../include/cutehmi/modbus/AbstractDevice.hpp"

namespace cutehmi {
namespace modbus {

AbstractDevice::AbstractDevice(QObject * parent):
	QObject(parent)
{
}

AbstractDevice::~AbstractDevice()
{
}

}
}

//(c)MP: Copyright � 2018, Michal Policht. All rights reserved.
//(c)MP: This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
