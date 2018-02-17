#include "Robot.h"

std::string WaitForGameData();


void Robot::AutonomousInit()
{
	//Zeroing the angle sensor and encoders
	ResetEncoders();
	AngleSensors.Reset();

	std::string gameData;
	try
	{
		gameData = WaitForGameData();
	}
	catch(std::string& error)
	{
		// If we didn't receive any game data, drive to the baseline
		DriverStation::GetInstance().ReportError("Unable to read game data. Driving to Baseline");

		Wait(SmartDashboard::GetNumber("Auto Delay", 0));
		DriveToBaseline();
		return;
	}

	// Add an optional delay to account for other robot auto paths
	Wait(SmartDashboard::GetNumber("Auto Delay", 0));

	switch(AutoLocationChooser->GetSelected())
	{
		case consts::AutoPosition::LEFT_START:
			switch(AutoObjectiveChooser->GetSelected())
			{
				case consts::AutoObjective::DEFAULT:
					SidePath(consts::AutoPosition::LEFT_START, gameData[0], gameData[1]);
					break;
				case consts::AutoObjective::SWITCH:
					break;
				case consts::AutoObjective::SCALE:
					break;
				case consts::AutoObjective::BASELINE:
					break;
				default:
					SidePath(consts::AutoPosition::LEFT_START, gameData[0], gameData[1]);
					break;
			}
			break;

		case consts::AutoPosition::RIGHT_START:
			switch(AutoObjectiveChooser->GetSelected())
			{
				case consts::AutoObjective::DEFAULT:
					SidePath(consts::AutoPosition::RIGHT_START, gameData[0], gameData[1]);
					break;
				case consts::AutoObjective::SWITCH:
					break;
				case consts::AutoObjective::SCALE:
					break;
				case consts::AutoObjective::BASELINE:
					break;
				default:
					SidePath(consts::AutoPosition::LEFT_START, gameData[0], gameData[1]);
					break;
			}
			break;

		case consts::AutoPosition::MIDDLE_START:
			MiddlePath(gameData[1]);
			break;

		default:
			DriveToBaseline();
			break;
	}
}

void Robot::AutonomousPeriodic()
{
	SmartDashboard::PutNumber("Angle", AngleSensors.GetAngle());
	SmartDashboard::PutNumber("Distance", PulsesToInches(FrontLeftMotor.GetSelectedSensorPosition(0)));
}

std::string WaitForGameData()
{
	std::string gameData;
	Timer gameDataTimer;
	gameDataTimer.Start();

	// Poll for the game data for some timeout period or until we've received the data
	while (gameData.length() == 0 && !gameDataTimer.HasPeriodPassed(consts::GAME_DATA_TIMOUT_S))
	{
		gameData = DriverStation::GetInstance().GetGameSpecificMessage();
		Wait(0.05);
	}
	gameDataTimer.Stop();

	if(gameData.length() == 0)
	{
		throw "Unable to read game data.";
	}
	else
	{
		return gameData;
	}
}

void Robot::DriveToBaseline()
{
	DriveDistance(85);
}

//Wait until the PID controller has reached the target and the robot is steady
void WaitUntilPIDSteady(PIDController& pidController, PIDSource& pidSource)
{
	double distance, diff, velocity;
	do
	{
		//Calculate velocity of the PID
		distance = pidSource.PIDGet();
		Wait(0.01);
		diff = pidSource.PIDGet() - distance;
		velocity = diff / 0.01;
	}
	while(!pidController.OnTarget() || abs(velocity) > 0.1);
	pidController.Disable();
}

void Robot::DriveDistance(double distance)
{
	//Disable other controllers
	AngleController.Disable();

	//Zeroing the angle sensor and encoders
	ResetEncoders();
	AngleSensors.Reset();

	//Disable test dist output for angle
	AnglePIDOut.SetTestDistOutput(0);
	//Make sure the PID objects know about each other to avoid conflicts
	DistancePID.SetAnglePID(&AnglePIDOut);
	AnglePIDOut.SetDistancePID(&DistancePID);

	//Configure the PID controller to make sure the robot drives straight with the NavX
	MaintainAngleController.Reset();
	MaintainAngleController.SetSetpoint(0);

	//Configure the robot to drive a given distance
	DistanceController.Reset();
	DistanceController.SetSetpoint(distance);

	MaintainAngleController.Enable();
	DistanceController.Enable();

	SmartDashboard::PutNumber("Target Distance", distance);

	WaitUntilPIDSteady(DistanceController, DistancePID);
}

void Robot::TurnAngle(double angle)
{
	//Disable other controllers
	DistanceController.Disable();
	MaintainAngleController.Disable();

	//Zeroing the angle sensor
	AngleSensors.Reset();

	//Disable test dist output for angle
	AnglePIDOut.SetTestDistOutput(0);

	//Remove the pointers since only one PID is being used
	DistancePID.SetAnglePID(nullptr);
	AnglePIDOut.SetDistancePID(nullptr);

	AngleController.Reset();
	AngleController.SetSetpoint(angle);
	AngleController.Enable();

	SmartDashboard::PutNumber("Target Angle", angle);

	WaitUntilPIDSteady(AngleController, AngleSensors);
}

void Robot::DriveFor(double seconds, double speed = 0.5)
{
	DriveTrain.ArcadeDrive(speed, 0);
	Wait(seconds);
	DriveTrain.ArcadeDrive(0, 0);
}

//Drives forward a distance, places a power cube, and then backs up
void Robot::DropCube(int driveSetpoint, consts::ElevatorIncrement elevatorSetpoint)
{
	DriveDistance(driveSetpoint);

	RaiseElevator(elevatorSetpoint);
	EjectCube();
	RaiseElevator(consts::ElevatorIncrement::GROUND);

	DriveDistance(-driveSetpoint);
}

void Robot::EjectCube()
{
	RightIntakeMotor.Set(1);
	LeftIntakeMotor.Set(-1);
	Wait(0.4);
	RightIntakeMotor.Set(0);
	LeftIntakeMotor.Set(0);
}

void Robot::RaiseElevator(consts::ElevatorIncrement elevatorSetpoint)
{
	ElevatorMotor.Set(0);
	ElevatorPIDController.SetSetpoint(consts::ELEVATOR_SETPOINTS[elevatorSetpoint]);
	ElevatorPIDController.Enable();

	WaitUntilPIDSteady(ElevatorPIDController, ElevatorPID);
}

//Puts the power cube in either the same side scale or same switch switch
void Robot::SidePath(consts::AutoPosition start, char switchPosition, char scalePosition)
{
	//90 for left, -90 for right
	double angle = (start == consts::AutoPosition::LEFT_START) ? 90 : -90;
	//L for left, R for right
	char startPosition = (start == consts::AutoPosition::LEFT_START) ? 'L' : 'R';

	//Cross the baseline
	DriveDistance(148);

	//Check if the switch is nearby, and if it is, place a cube in it
	if(switchPosition == startPosition)
	{
		TurnAngle(angle);
		DropCube(5, consts::ElevatorIncrement::SWITCH);

		return; //End auto just in case the cube misses
	}

	//Otherwise, go forward to a better position
	DriveDistance(100);

	//Check if the scale is nearby, and if it is, place a cube in it
	if(scalePosition == startPosition)
	{
		TurnAngle(-angle);
		DriveDistance(14);

		TurnAngle(angle);
		DriveDistance(56);

		TurnAngle(angle);
		DropCube(5, consts::ElevatorIncrement::SCALE);

		return; //End auto just in case the cube misses
	}
}

//Puts a power cube in the switch on the side opposite of the robot
void Robot::OppositeSwitch(consts::AutoPosition start)
{
	//90 for left, -90 for right
	double angle = (start == consts::AutoPosition::LEFT_START) ? 90 : -90;

	if(SwitchApproachChooser->GetSelected() == consts::SwitchApproach::FRONT)
	{
		DriveDistance(42.5);
		TurnAngle(angle);

		DriveDistance(155);
		TurnAngle(-angle);

		TurnAngle(angle);
		DropCube(59, consts::ElevatorIncrement::SWITCH);
	}
	else
	{
		DriveDistance(211);
		TurnAngle(angle);

		DriveDistance(200);
		TurnAngle(angle);

		DriveDistance(62.5);
		TurnAngle(angle);

		DropCube(5, consts::ElevatorIncrement::SWITCH);
	}
}

//Puts a power cube in the scale on the side opposite of the robot
void Robot::OppositeScale(consts::AutoPosition start)
{
	//90 for left, -90 for right
	double angle = (start == consts::AutoPosition::LEFT_START) ? 90 : -90;

	DriveDistance(211);
	TurnAngle(angle);

	DriveDistance(200);
	TurnAngle(-angle);

	DriveDistance(37.5);
	TurnAngle(angle);

	DriveDistance(14);
	TurnAngle(-angle);

	DriveDistance(56);

	TurnAngle(angle);
	DropCube(5, consts::ElevatorIncrement::SCALE);
}

//Puts a power cube in the switch from the middle position
void Robot::MiddlePath(char switchPosition)
{
	double angle = 90;

	//Go forward
	DriveDistance(42.5);

	//Check which way the cube should be placed
	if(SwitchApproachChooser->GetSelected() == consts::SwitchApproach::FRONT)
	{
		//If the cube is being placed from the front
		if(switchPosition == 'L')
		{
			TurnAngle(-angle);
			DriveDistance(80);

			TurnAngle(angle);
			DropCube(78, consts::ElevatorIncrement::SWITCH);
		}
		else if(switchPosition == 'R')
		{
			TurnAngle(angle);
			DriveDistance(29);

			TurnAngle(-angle);
			DropCube(78, consts::ElevatorIncrement::SWITCH);
		}
	}
	else
	{
		//If the cube is being placed from the side
		if(switchPosition == 'L')
		{
			TurnAngle(-angle);
			DriveDistance(126);

			TurnAngle(angle);
			DriveDistance(106);

			TurnAngle(angle);
			DropCube(5, consts::ElevatorIncrement::SWITCH);
		}
		else if(switchPosition == 'R')
		{
			TurnAngle(angle);
			DriveDistance(74);

			TurnAngle(-angle);
			DriveDistance(106);

			TurnAngle(angle);
			DropCube(5, consts::ElevatorIncrement::SWITCH);
		}
	}
}
