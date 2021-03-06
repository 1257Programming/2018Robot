#include "Robot.h"

Robot::Robot() :
	BackRightMotor(1),
	FrontRightMotor(2),
	FrontLeftMotor(3),
	BackLeftMotor(4),
	RightIntakeMotor(6),
	LeftIntakeMotor(8),
	RightElevatorMotor(7),
	LeftElevatorMotor(9),
	LinkageMotor(5),
	LeftMotors(FrontLeftMotor, BackLeftMotor),
	RightMotors(FrontRightMotor, BackRightMotor),
	DriveTrain(LeftMotors, RightMotors),
	DriveController(0),
	OperatorController(1),
	AngleSensors(SPI::Port::kMXP, SPI::kOnboardCS0),

	LeftSolenoid(1, 0),
	RightSolenoid(3, 2),

	ElevatorPID(&RightElevatorMotor, &LeftElevatorMotor),
	AnglePIDOut(DriveTrain),
	DistancePID(FrontLeftMotor, DriveTrain),
	AngleController(0.04, 0, 0.04, AngleSensors, AnglePIDOut), //(0.02525, 0, 0.025)
	MaintainAngleController(0.04, 0.0, 0.0, AngleSensors, AnglePIDOut), //(0.03, 0.0015, 0.06)
	DistanceController(0.03, 0, 0.06, DistancePID, DistancePID), //(0.04, 0, 0)
	ElevatorPIDController(0.25, 0., 0., ElevatorPID, ElevatorPID),
	m_isElevatorLowering(false),
	m_isElevatorInAutoMode(false),
	m_targetElevatorStep(0),
	EjectTimer()
{
	AutoLocationChooser = new SendableChooser<consts::AutoPosition>();
	AutoObjectiveChooser = new SendableChooser<consts::AutoObjective>();
	SwitchApproachChooser = new SendableChooser<consts::SwitchApproach>();
}

Robot::~Robot()
{
	delete AutoLocationChooser;
	delete AutoObjectiveChooser;
	delete SwitchApproachChooser;
}

void Robot::RobotInit()
{
	StopCurrentProcesses();
	RightElevatorMotor.SetSelectedSensorPosition(0, consts::PID_LOOP_ID, consts::TALON_TIMEOUT_MS);
	RightElevatorMotor.SetNeutralMode(Brake);
	RightIntakeMotor.SetNeutralMode(Brake);
	LeftIntakeMotor.SetNeutralMode(Brake);

	// Current limiting
	FrontLeftMotor.ConfigContinuousCurrentLimit(consts::FORTY_AMP_FUSE_CONT_MAX, consts::CONT_CURRENT_TIMEOUT_MS);
	FrontLeftMotor.EnableCurrentLimit(true);

	FrontRightMotor.ConfigContinuousCurrentLimit(consts::FORTY_AMP_FUSE_CONT_MAX, consts::CONT_CURRENT_TIMEOUT_MS);
	FrontRightMotor.EnableCurrentLimit(true);

	BackLeftMotor.ConfigContinuousCurrentLimit(consts::FORTY_AMP_FUSE_CONT_MAX, consts::CONT_CURRENT_TIMEOUT_MS);
	BackLeftMotor.EnableCurrentLimit(true);

	BackRightMotor.ConfigContinuousCurrentLimit(consts::FORTY_AMP_FUSE_CONT_MAX, consts::CONT_CURRENT_TIMEOUT_MS);
	BackRightMotor.EnableCurrentLimit(true);

	RightIntakeMotor.ConfigContinuousCurrentLimit(consts::FORTY_AMP_FUSE_CONT_MAX, consts::CONT_CURRENT_TIMEOUT_MS);
	RightIntakeMotor.EnableCurrentLimit(true);

	LeftIntakeMotor.ConfigContinuousCurrentLimit(consts::THIRTY_AMP_FUSE_CONT_MAX, consts::CONT_CURRENT_TIMEOUT_MS);
	LeftIntakeMotor.EnableCurrentLimit(true);

//	RightElevatorMotor.ConfigContinuousCurrentLimit(consts::ELEVATOR_CONT_CURRENT_MAX, consts::ELEVATOR_CONT_CURRENT_TIMEOUT_MS);
//	RightElevatorMotor.EnableCurrentLimit(true);

	// Linkage and Elevator Setup

	RightElevatorMotor.ConfigSelectedFeedbackSensor(FeedbackDevice::CTRE_MagEncoder_Relative, consts::PID_LOOP_X, consts::TIMEOUT_MS);
	RightElevatorMotor.SetSensorPhase(true);

	// Configuring the Talon Drive Encoders
	FrontLeftMotor.ConfigSelectedFeedbackSensor(FeedbackDevice::CTRE_MagEncoder_Relative, consts::PID_LOOP_ID, consts::TALON_TIMEOUT_MS);
	FrontLeftMotor.SetSensorPhase(true);

	FrontRightMotor.ConfigSelectedFeedbackSensor(FeedbackDevice::CTRE_MagEncoder_Relative, consts::PID_LOOP_ID, consts::TALON_TIMEOUT_MS);
	FrontRightMotor.SetSensorPhase(true);

	// Adding PID Controllers to LiveWindow
	LiveWindow::GetInstance()->Add(&AngleController);
	LiveWindow::GetInstance()->Add(&MaintainAngleController);
	LiveWindow::GetInstance()->Add(&DistanceController);

	// Configuring Angle PID Controller
	AngleController.SetAbsoluteTolerance(1);
	AngleController.SetOutputRange(-0.3, 0.3);

	// Configuring Maintain Angle PID Controller
	MaintainAngleController.SetAbsoluteTolerance(0.5);
	MaintainAngleController.SetOutputRange(-1.0, 1.0);

	// Configuring Distance PID Controller
	DistanceController.SetAbsoluteTolerance(3.5);
	DistanceController.SetOutputRange(-0.7, 0.7);

	// Setup camera stream in a separate thread
	std::thread visionThread(VisionThread);
	visionThread.detach();

	// Configure the SendableChoosers for auto
	AutoLocationChooser->AddObject("Left Start", consts::AutoPosition::LEFT_START);
	AutoLocationChooser->AddDefault("Middle Start", consts::AutoPosition::MIDDLE_START);
	AutoLocationChooser->AddObject("Right Start", consts::AutoPosition::RIGHT_START);

	AutoObjectiveChooser->AddDefault("Default", consts::AutoObjective::DEFAULT);
	AutoObjectiveChooser->AddObject("Switch", consts::AutoObjective::SWITCH);
	AutoObjectiveChooser->AddObject("Scale", consts::AutoObjective::SCALE);
	AutoObjectiveChooser->AddObject("Baseline", consts::AutoObjective::BASELINE);

	SwitchApproachChooser->AddDefault("Angle Shot", consts::SwitchApproach::FRONT);
	SwitchApproachChooser->AddObject("Side Shot", consts::SwitchApproach::SIDE);

	// Send the sendable choosers to SmartDashboard
	SmartDashboard::PutData("Auto Position", AutoLocationChooser);
	SmartDashboard::PutData("Auto Objective", AutoObjectiveChooser);
	SmartDashboard::PutData("Switch Approach", SwitchApproachChooser);
	SmartDashboard::PutNumber("Auto Delay", 0);
}

START_ROBOT_CLASS(Robot)
