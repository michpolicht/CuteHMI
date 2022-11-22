#include <cutehmi/services/ServiceGroup.hpp>
#include <cutehmi/services/ServiceAutoActivate.hpp>

#include "internal/ServiceStateMachine.hpp"
#include "internal/ServiceStateInterface.hpp"

#include <list>

namespace cutehmi {
namespace services {

void ServiceGroup::PostConditionCheckEvent(QStateMachine * stateMachine)
{
	if (stateMachine)
		stateMachine->postEvent(new QEvent(QEvent::Type(CONDITION_CHECK_EVENT)));
	else
		CUTEHMI_WARNING("Could not post condition check event, because given state machine pointer is null.");
}

ServiceGroup::ServiceGroup(QObject * parent):
	AbstractService(std::make_unique<internal::ServiceStateInterface>(), DefaultStatus(), parent, & DefaultControllers()),
	m(new Members(this))
{
	connect(this, & ServiceGroup::stoppedCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::startingCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::startedCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::stoppingCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::brokenCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::repairingCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::evacuatingCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::interruptedCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::yieldingCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::activeCountChanged, this, & ServiceGroup::handleCounters);
	connect(this, & ServiceGroup::idlingCountChanged, this, & ServiceGroup::handleCounters);
}

ServiceGroup::~ServiceGroup()
{
	// Stop the service.
	stop();

	if (m->stateMachine)
		m->stateMachine->shutdown();

	destroyStateMachine();

	clearServices();
}

int ServiceGroup::startedCount() const
{
	return m->startedCount;
}

int ServiceGroup::startingCount() const
{
	return m->startingCount;
}

int ServiceGroup::stoppingCount() const
{
	return m->stoppingCount;
}

int ServiceGroup::stoppedCount() const
{
	return m->stoppedCount;
}

int ServiceGroup::brokenCount() const
{
	return m->brokenCount;
}

int ServiceGroup::repairingCount() const
{
	return m->repairingCount;
}

int ServiceGroup::evacuatingCount() const
{
	return m->evacuatingCount;
}

int ServiceGroup::interruptedCount() const
{
	return m->interruptedCount;
}

int ServiceGroup::activeCount() const
{
	return m->activeCount;
}

int ServiceGroup::yieldingCount() const
{
	return m->yieldingCount;
}

int ServiceGroup::idlingCount() const
{
	return m->idlingCount;
}

QQmlListProperty<ServiceGroupRule> ServiceGroup::ruleList()
{
	return m->ruleList;
}

void ServiceGroup::appendRule(ServiceGroupRule * rule)
{
	RuleListAppend(& m->ruleList, rule);
}

void ServiceGroup::clearRules()
{
	RuleListClear(& m->ruleList);
}

QQmlListProperty<AbstractService> ServiceGroup::serviceList()
{
	return m->serviceList;
}

void ServiceGroup::appendService(AbstractService * service)
{
	ServiceListAppend(& m->serviceList, service);
}

void ServiceGroup::clearServices()
{
	ServiceListClear(& m->serviceList);
}

void ServiceGroup::configureStarting(QState * starting, AssignStatusFunction assignStatus)
{
	configureStartingOrRepairing(starting, assignStatus);
}

void ServiceGroup::configureStarted(QState * active, const QState * idling, const QState * yielding, AssignStatusFunction assignStatus)
{
	Q_UNUSED(idling)
	Q_UNUSED(yielding)
	Q_UNUSED(assignStatus)

	active->setChildMode(QState::ParallelStates);

	for (auto && service : m->services) {
		QState * serviceSequence = new QState(active);

		QState * serviceActive = new QState(serviceSequence);
		connect(serviceActive, & QState::entered, service, & AbstractService::activate);

		std::list<std::unique_ptr<QAbstractTransition>> transitionList;
		for (auto && rule : m->rules) {
			auto transition = rule->conditionalTransition(ServiceGroupRule::SERVICE_ACTIVATE, service);
			if (transition)
				transitionList.push_back(std::move(transition));
		}

		QState * lastState = serviceActive;
		while (!transitionList.empty()) {
			auto transition = std::move(transitionList.back());
			transitionList.pop_back();
			transition->setTargetState(lastState);
			QState * conditionWait = new QState(serviceSequence);
			conditionWait->addTransition(transition.release());
			lastState = conditionWait;
		}

		serviceSequence->setInitialState(lastState);
	}
}

void ServiceGroup::configureStopping(QState * stopping, AssignStatusFunction assignStatus)
{
	configureStoppingOrEvacuating(stopping, assignStatus);
}

void ServiceGroup::configureBroken(QState * broken, AssignStatusFunction assignStatus)
{
	Q_UNUSED(broken)
	Q_UNUSED(assignStatus)
}

void ServiceGroup::configureRepairing(QState * repairing, AssignStatusFunction assignStatus)
{
	configureStartingOrRepairing(repairing, assignStatus);

	// Services might have been repaired on their while ServiceGroup was in broken state, so make additional call to
	// handleCounters() whenever repairing state is re-entered.

	//<CuteHMI.Services-5.workaround target="Qt" cause="bug">
	// Function handleCounters() relies on values returned by QState::active(). Unfortunately `active` property is being set to true
	// only after the signal QState::entered() has been emitted, so if we connected QState::entered() to handleCounters, then despite
	// repairing state has been entered `active` property would return false. A workaround is to rely on QState::activeChanged()
	// signal instead of QState::entered().
	// Instead of:
	// connect(repairing, & QState::entered, this, & ServiceGroup::handleCounters);
	connect(repairing, & QState::activeChanged, this, & ServiceGroup::handleCounters);
	//</CuteHMI.Services-5.workaround target="Qt" cause="bug">
}

void ServiceGroup::configureEvacuating(QState * evacuating, AssignStatusFunction assignStatus)
{
	configureStoppingOrEvacuating(evacuating, assignStatus);
}

std::unique_ptr<QAbstractTransition> ServiceGroup::transitionToStarted() const
{
	return std::make_unique<QSignalTransition>(this, & ServiceGroup::groupStarted);
}

std::unique_ptr<QAbstractTransition> ServiceGroup::transitionToStopped() const
{
	return std::make_unique<QSignalTransition>(this, & ServiceGroup::groupStopped);
}

std::unique_ptr<QAbstractTransition> ServiceGroup::transitionToBroken() const
{
	return std::make_unique<QSignalTransition>(this, & ServiceGroup::groupBroken);
}

std::unique_ptr<QAbstractTransition> ServiceGroup::transitionToYielding() const
{
	return nullptr;
}

std::unique_ptr<QAbstractTransition> ServiceGroup::transitionToIdling() const
{
	return nullptr;
}

void ServiceGroup::classBegin()
{
	m->qmlBeingParsed = true;
}

void ServiceGroup::componentComplete()
{
	m->qmlBeingParsed = false;

	initializeStateMachine();
}

void ServiceGroup::postConditionCheckEvent() const
{
	if (m->stateMachine)
		PostConditionCheckEvent(m->stateMachine);
	else
		CUTEHMI_WARNING("Could not post condition check event, because state machine has not been initialized yet.");
}

const AbstractService::ControllersContainer & ServiceGroup::DefaultControllers()
{
	static ServiceAutoActivate defaultAutoActivate;
	static ControllersContainer defaultControllers = {& defaultAutoActivate};
	return defaultControllers;
}

void ServiceGroup::setStartedCount(int count)
{
	if (m->startedCount != count) {
		m->startedCount = count;
		emit startedCountChanged();
	}
}

void ServiceGroup::setStartingCount(int count)
{
	if (m->startingCount != count) {
		m->startingCount = count;
		emit startingCountChanged();
	}
}

void ServiceGroup::setStoppingCount(int count)
{
	if (m->stoppingCount != count) {
		m->stoppingCount = count;
		emit stoppingCountChanged();
	}
}

void ServiceGroup::setStoppedCount(int count)
{
	if (m->stoppedCount != count) {
		m->stoppedCount = count;
		emit stoppedCountChanged();
	}
}

void ServiceGroup::setBrokenCount(int count)
{
	if (m->brokenCount != count) {
		m->brokenCount = count;
		emit brokenCountChanged();
	}
}

void ServiceGroup::setRepairingCount(int count)
{
	if (m->repairingCount != count) {
		m->repairingCount = count;
		emit repairingCountChanged();
	}
}

void ServiceGroup::setEvacuatingCount(int count)
{
	if (m->evacuatingCount != count) {
		m->evacuatingCount = count;
		emit evacuatingCountChanged();
	}
}

void ServiceGroup::setInterruptedCount(int count)
{
	if (m->interruptedCount != count) {
		m->interruptedCount = count;
		emit interruptedCountChanged();
	}
}

void ServiceGroup::setActiveCount(int count)
{
	if (m->activeCount != count) {
		m->activeCount = count;
		emit activeCountChanged();
	}
}

void ServiceGroup::setYieldingCount(int count)
{
	if (m->yieldingCount != count) {
		m->yieldingCount = count;
		emit yieldingCountChanged();
	}
}

void ServiceGroup::setIdlingCount(int count)
{
	if (m->idlingCount != count) {
		m->idlingCount = count;
		emit idlingCountChanged();
	}
}

const ServiceGroup::RulesContainer & ServiceGroup::rules() const
{
	return m->rules;
}

ServiceGroup::RulesContainer & ServiceGroup::rules()
{
	return m->rules;
}

const ServiceGroup::ServicesContainer & ServiceGroup::services() const
{
	return m->services;
}

ServiceGroup::ServicesContainer & ServiceGroup::services()
{
	return m->services;
}

void ServiceGroup::handleCounters()
{
	if (states()->starting()->active())
		if (yieldingCount() == m->services.count())
			emit groupStarted();

	if (states()->repairing()->active())
		if (startedCount() == m->services.count())
			emit groupStarted();

	if (states()->stopping()->active() || states()->evacuating()->active())
		if (stoppedCount() == m->services.count())
			emit groupStopped();

	if (!states()->repairing()->active())
		if (brokenCount() > 0)
			emit groupBroken();

	if (interruptedCount() > 0)
		emit groupBroken();
}

QString & ServiceGroup::DefaultStatus()
{
	static QString name = QObject::tr("Uninitialized");
	return name;
}

workarounds::qt5compatibility::sizeType ServiceGroup::RuleListCount(QQmlListProperty<ServiceGroupRule> * property)
{
	return static_cast<RulesContainer *>(property->data)->count();
}

ServiceGroupRule * ServiceGroup::RuleListAt(QQmlListProperty<ServiceGroupRule> * property, workarounds::qt5compatibility::sizeType index)
{
	return static_cast<RulesContainer *>(property->data)->value(index);
}

void ServiceGroup::RuleListClear(QQmlListProperty<ServiceGroupRule> * property)
{
	static_cast<RulesContainer *>(property->data)->clear();
}

void ServiceGroup::RuleListAppend(QQmlListProperty<ServiceGroupRule> * property, ServiceGroupRule * value)
{
	static_cast<RulesContainer *>(property->data)->append(value);
}

workarounds::qt5compatibility::sizeType ServiceGroup::ServiceListCount(QQmlListProperty<AbstractService> * property)
{
	return static_cast<ServicesContainer *>(property->data)->count();
}

AbstractService * ServiceGroup::ServiceListAt(QQmlListProperty<AbstractService> * property, workarounds::qt5compatibility::sizeType index)
{
	return static_cast<ServicesContainer *>(property->data)->value(index);
}

void ServiceGroup::ServiceListClear(QQmlListProperty<AbstractService> * property)
{
	ServiceGroup * serviceGroup = static_cast<ServiceGroup *>(property->object);

	for (ServicesContainer::iterator it = serviceGroup->services().begin(); it != serviceGroup->services().end(); ++it)
		DeleteConnectionDataEntry(serviceGroup->m->serviceConnections, *it);

	serviceGroup->m->serviceConnections.clear();

	static_cast<ServicesContainer *>(property->data)->clear();

	serviceGroup->destroyStateMachine();
}

void ServiceGroup::ServiceListAppend(QQmlListProperty<AbstractService> * property, AbstractService * value)
{
	ServiceGroup * serviceGroup = static_cast<ServiceGroup *>(property->object);

	ConnectionData * connectionData = CreateConnectionDataEntry(serviceGroup->m->serviceConnections, value);
	ConnectStateCounters(*connectionData, serviceGroup, value);

	static_cast<ServicesContainer *>(property->data)->append(value);

	if (!serviceGroup->m->qmlBeingParsed) {
		if (!serviceGroup->m->stateMachine)
			serviceGroup->initializeStateMachine();
		else {
			serviceGroup->m->stateMachine->reconfigureStarting();
			serviceGroup->m->stateMachine->reconfigureStarted();
			serviceGroup->m->stateMachine->reconfigureStopping();
			serviceGroup->m->stateMachine->reconfigureBroken();
			serviceGroup->m->stateMachine->reconfigureRepairing();
			serviceGroup->m->stateMachine->reconfigureEvacuating();
		}
	}
}

void ServiceGroup::ConnectStateCounters(ConnectionData & connectionData, ServiceGroup * serviceGroup, AbstractService * service)
{
	struct StateConnectionParams {
		ConnectionsContainer * connections;
		int (ServiceGroup::*getter)(void) const;
		void (ServiceGroup::*setter)(int);
		union {
			QAbstractState * (StateInterface::*stateGetter)(void) const;
			QAbstractState * (StartedStateInterface::*startedStateGetter)(void) const;
		};
		union {
			void (StateInterface::*stateChangedSignal)(void);
			void (StartedStateInterface::*startedSubstateChangedSignal)(void);
		};
		bool startedSubstate = false;
	};

	QList<StateConnectionParams> stateConnectionParamsList;

	stateConnectionParamsList << StateConnectionParams{
		& connectionData.stopped,
		& ServiceGroup::stoppedCount,
		& ServiceGroup::setStoppedCount,
		{ & StateInterface::stopped },
		{ & StateInterface::startedChanged },
	} << StateConnectionParams{
		& connectionData.starting,
		& ServiceGroup::startingCount,
		& ServiceGroup::setStartingCount,
		{ & StateInterface::starting },
		{ & StateInterface::startingChanged },
	} << StateConnectionParams{
		& connectionData.started,
		& ServiceGroup::startedCount,
		& ServiceGroup::setStartedCount,
		{ & StateInterface::started },
		{ & StateInterface::startedChanged },
	} << StateConnectionParams{
		& connectionData.stopping,
		& ServiceGroup::stoppingCount,
		& ServiceGroup::setStoppingCount,
		{ & StateInterface::stopping },
		{ & StateInterface::stoppingChanged },
	} << StateConnectionParams{
		& connectionData.broken,
		& ServiceGroup::brokenCount,
		& ServiceGroup::setBrokenCount,
		{ & StateInterface::broken },
		{ & StateInterface::brokenChanged },
	} << StateConnectionParams{
		& connectionData.repairing,
		& ServiceGroup::repairingCount,
		& ServiceGroup::setRepairingCount,
		{ & StateInterface::repairing },
		{	& StateInterface::repairingChanged },
	} << StateConnectionParams{
		& connectionData.evacuating,
		& ServiceGroup::evacuatingCount,
		& ServiceGroup::setEvacuatingCount,
		{ & StateInterface::evacuating },
		{ & StateInterface::evacuatingChanged },
	} << StateConnectionParams{
		& connectionData.interrupted,
		& ServiceGroup::interruptedCount,
		& ServiceGroup::setInterruptedCount,
		{ & StateInterface::interrupted },
		{ & StateInterface::interruptedChanged },
	};

	// Designated initializers are not available until C++20, so lets initialize started substates verbosely.
	StateConnectionParams yieldingConnectionParams;
	yieldingConnectionParams.connections = & connectionData.yielding;
	yieldingConnectionParams.getter = & ServiceGroup::yieldingCount;
	yieldingConnectionParams.setter = & ServiceGroup::setYieldingCount;
	yieldingConnectionParams.startedStateGetter = & StartedStateInterface::yielding;
	yieldingConnectionParams.startedSubstateChangedSignal = & StartedStateInterface::yieldingChanged;
	yieldingConnectionParams.startedSubstate = true;
	stateConnectionParamsList << yieldingConnectionParams;

	StateConnectionParams activeConnectionParams;
	activeConnectionParams.connections = & connectionData.active;
	activeConnectionParams.getter = & ServiceGroup::activeCount;
	activeConnectionParams.setter = & ServiceGroup::setActiveCount;
	activeConnectionParams.startedStateGetter = & StartedStateInterface::active;
	activeConnectionParams.startedSubstateChangedSignal = & StartedStateInterface::activeChanged;
	activeConnectionParams.startedSubstate = true;
	stateConnectionParamsList << activeConnectionParams;

	StateConnectionParams idlingConnectionParams;
	idlingConnectionParams.connections = & connectionData.idling;
	idlingConnectionParams.getter = & ServiceGroup::idlingCount;
	idlingConnectionParams.setter = & ServiceGroup::setIdlingCount;
	idlingConnectionParams.startedStateGetter = & StartedStateInterface::idling;
	idlingConnectionParams.startedSubstateChangedSignal = & StartedStateInterface::idlingChanged;
	idlingConnectionParams.startedSubstate = true;
	stateConnectionParamsList << idlingConnectionParams;

	for (auto && params : stateConnectionParamsList) {
		auto reconnectStateCounterLambda = [serviceGroup, service, params]() {
			QAbstractState * state;
			if (params.startedSubstate)
				state = (service->states()->startedStates()->*params.startedStateGetter)();
			else
				state = (service->states()->*params.stateGetter)();
			ReconnectStateCounter(serviceGroup, *params.connections, params.getter, params.setter, *state);
		};
		reconnectStateCounterLambda();
		if (params.startedSubstate)
			connectionData.stateChanged.append(connect(service->states()->startedStates(), params.startedSubstateChangedSignal, serviceGroup, reconnectStateCounterLambda));
		else
			connectionData.stateChanged.append(connect(service->states(), params.stateChangedSignal, serviceGroup, reconnectStateCounterLambda));
	}
}

void ServiceGroup::ReconnectStateCounter(ServiceGroup * serviceGroup,
		ConnectionsContainer & connections,
		int (ServiceGroup::*getter)(void) const,
		void (ServiceGroup::*setter)(int),
		const QAbstractState & state)
{
	ClearConnections(connections);

	connections.append(connect(& state, & QAbstractState::activeChanged, serviceGroup, [getter, setter, serviceGroup](bool active) {
		(serviceGroup->*setter)((serviceGroup->*getter)() + (active ? 1 : -1));
	}));
}

void ServiceGroup::ClearConnections(ConnectionsContainer & connections)
{
	for (auto && connection : connections)
		disconnect(connection);

	connections.clear();
}

ServiceGroup::ConnectionData * ServiceGroup::CreateConnectionDataEntry(ServiceConnectionsContainer & serviceConnections, AbstractService * service)
{
	ConnectionData * connectionData = new ConnectionData;
	serviceConnections.insert(service, connectionData);
	return connectionData;
}

void ServiceGroup::DeleteConnectionDataEntry(ServiceConnectionsContainer & serviceConnections, AbstractService * service)
{
	ConnectionData * connectionData = serviceConnections.value(service);
	ClearConnections(connectionData->stopped);
	ClearConnections(connectionData->starting);
	ClearConnections(connectionData->started);
	ClearConnections(connectionData->stopping);
	ClearConnections(connectionData->broken);
	ClearConnections(connectionData->repairing);
	ClearConnections(connectionData->evacuating);
	ClearConnections(connectionData->interrupted);
	ClearConnections(connectionData->yielding);
	ClearConnections(connectionData->active);
	ClearConnections(connectionData->idling);
	ClearConnections(connectionData->stateChanged);
	delete connectionData;
}

internal::ServiceStateInterface * ServiceGroup::stateInterface() const
{
	return static_cast<internal::ServiceStateInterface *>(states());
}

void ServiceGroup::initializeStateMachine(bool start)
{
	try {
		m->stateMachine = new internal::ServiceStateMachine(this, this);

		// Service status is read-only property, thus it is updated through state machine writebale double.
		connect(m->stateMachine, & internal::ServiceStateMachine::statusChanged, this, & ServiceGroup::setStatus);

		stateInterface()->bindStateMachine(m->stateMachine);

		if (start)
			m->stateMachine->start();	// Note: start() function is shadowed by internal::ServiceStateMachine.

		emit initialized();
	} catch (const std::exception & e) {
		CUTEHMI_CRITICAL("Service '" << name() << "' could not initialize new state machine, because of the following exception: " << e.what());
	}
}

void ServiceGroup::destroyStateMachine()
{
	if (m->stateMachine) {
		stateInterface()->unbindStateMachine();

		m->stateMachine->stop();
		m->stateMachine->deleteLater();
		m->stateMachine = nullptr;
	}
}

void ServiceGroup::configureStoppingOrEvacuating(QState * state, AssignStatusFunction assignStatus)
{
	Q_UNUSED(assignStatus)

	state->setChildMode(QState::ParallelStates);

	for (auto && service : m->services) {
		QState * serviceSequence = new QState(state);

		QState * serviceStopping = new QState(serviceSequence);
		connect(serviceStopping, & QState::entered, service, & AbstractService::stop);

		std::list<std::unique_ptr<QAbstractTransition>> transitionList;
		for (auto && rule : m->rules) {
			auto transition = rule->conditionalTransition(ServiceGroupRule::SERVICE_STOP, service);
			if (transition)
				transitionList.push_back(std::move(transition));
		}

		QState * lastState = serviceStopping;
		while (!transitionList.empty()) {
			auto transition = std::move(transitionList.back());
			transitionList.pop_back();
			transition->setTargetState(lastState);
			QState * conditionWait = new QState(serviceSequence);
			conditionWait->addTransition(transition.release());
			lastState = conditionWait;
		}

		serviceSequence->setInitialState(lastState);
	}
}

void ServiceGroup::configureStartingOrRepairing(QState * state, AssignStatusFunction assignStatus)
{
	Q_UNUSED(assignStatus)

	state->setChildMode(QState::ParallelStates);

	for (auto && service : m->services) {
		QState * serviceSequence = new QState(state);

		QState * startService = new QState(serviceSequence);
		connect(startService, & QState::entered, service, & AbstractService::start);

		std::list<std::unique_ptr<QAbstractTransition>> transitionList;
		for (auto && rule : m->rules) {
			auto transition = rule->conditionalTransition(ServiceGroupRule::SERVICE_START, service);
			if (transition)
				transitionList.push_back(std::move(transition));
		}

		QState * lastState = startService;
		while (!transitionList.empty()) {
			auto transition = std::move(transitionList.back());
			transitionList.pop_back();
			transition->setTargetState(lastState);
			QState * conditionWait = new QState(serviceSequence);
			connect(conditionWait, & QState::entered, this, & ServiceGroup::postConditionCheckEvent);
			conditionWait->addTransition(transition.release());
			lastState = conditionWait;
		}

		serviceSequence->setInitialState(lastState);
	}
}

}
}