#include "Robot.h"

void Robot::TeleopInit()
{

}

void Robot::TeleopPeriodic()
{
	double speedVal = 0;
	double turnVal = 0;

	// If they press A, use single stick arcade with the left joystick
	if(DriveController.GetAButton())
	{
		speedVal = DriveController.GetY(GenericHID::JoystickHand::kLeftHand);
		turnVal = DriveController.GetX(GenericHID::JoystickHand::kLeftHand);
	}
	// If they press the left bumper, use the left joystick for forward and
	// backward motion and the right joystick for turning
	else if(DriveController.GetBumper(GenericHID::JoystickHand::kLeftHand))
	{
		speedVal = DriveController.GetY(GenericHID::JoystickHand::kLeftHand);
		turnVal = DriveController.GetX(GenericHID::JoystickHand::kRightHand);
	}
	// If they press the right bumper, use the right joystick for forward and
	// backward motion and the left joystick for turning
	else if(DriveController.GetBumper(GenericHID::JoystickHand::kRightHand))
	{
		speedVal = DriveController.GetY(GenericHID::JoystickHand::kRightHand);
		turnVal = DriveController.GetX(GenericHID::JoystickHand::kLeftHand);
	}

	DriveTrain.ArcadeDrive(speedVal, turnVal);

	// Encoder testing
	SmartDashboard::PutNumber("Front Left Encoder", FrontLeftMotor.GetSelectedSensorPosition(0));
	SmartDashboard::PutNumber("Front Right Encoder", FrontRightMotor.GetSelectedSensorPosition(0));
	SmartDashboard::PutNumber("Back Left Encoder", BackLeftMotor.GetSelectedSensorPosition(0));
	SmartDashboard::PutNumber("Back Right Encoder", BackRightMotor.GetSelectedSensorPosition(0));
}
