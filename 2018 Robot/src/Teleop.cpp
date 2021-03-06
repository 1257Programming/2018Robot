#include "Robot.h"

double Robot::GetClosestStepNumber()
{
	double currentHeight = ElevatorPID.GetHeightInches();
	for(int i = 0; i < consts::NUM_ELEVATOR_SETPOINTS; i++)
	{
		// If the elevator is directly below a given setpoint, go to that setpoint
		if(currentHeight < consts::ELEVATOR_SETPOINTS[i])
		{
			return i;
		}
	}
	return consts::NUM_ELEVATOR_SETPOINTS - 1;
}

bool isApproachingMechanicalStop(double output, double currentHeight)
{
	// If the elevator is moving down towards the bottom
	if(output < 0 && currentHeight < consts::ELEVATOR_SETPOINTS[consts::ElevatorIncrement::SCALE_LOW])
	{
		return true;
	}
	// If the elevator is moving up towards the top
	else if (output > 0 && currentHeight > consts::ELEVATOR_SETPOINTS[consts::ElevatorIncrement::SCALE_HIGH])
	{
		return true;
	}
	else
	{
		return false;
	}
}

// Prevent the elevator from reaching its hard stops
double Robot::CapElevatorOutput(double output, bool safetyModeEnabled)
{
	double currentHeight = ElevatorPID.GetHeightInches();

	// If we're trying to run the elevator down after reaching the bottom or trying
	// to run it up after reaching the max height, set the motor output to 0
	if((output < 0 && currentHeight < consts::ELEVATOR_SETPOINTS[consts::ElevatorIncrement::GROUND]))
			// ||(output > 0 && currentHeight > consts::ELEVATOR_SETPOINTS[consts::ElevatorIncrement::MAX_HEIGHT]))
	{
		output = 0;
	}
	else if(safetyModeEnabled)
	{
		// If the elevator is approaching a mechanical stop, slow down the motor
		if(isApproachingMechanicalStop(output, currentHeight))
		{
			output *= consts::ELEVATOR_SPEED_REDUCTION;
		}
	}

	return output;
}

bool Robot::IsElevatorTooHigh()
{
	return ElevatorPID.PIDGet() > 12.5;
}

//Driver Controls
void Robot::Drive()
{
	double forwardSpeed = 0;
	double turnSpeed = 0;

	// If they press A, use single stick arcade with the left joystick
	if(DriveController.GetAButton())
	{
		forwardSpeed = DriveController.GetY(GenericHID::JoystickHand::kLeftHand);
		turnSpeed = DriveController.GetX(GenericHID::JoystickHand::kLeftHand);
	}
	// If they press the left bumper, use the left joystick for forward and
	// backward motion and the right joystick for turning
	else if(DriveController.GetBumper(GenericHID::JoystickHand::kLeftHand))
	{
		forwardSpeed = DriveController.GetY(GenericHID::JoystickHand::kLeftHand);
		turnSpeed = DriveController.GetX(GenericHID::JoystickHand::kRightHand);
	}
	// If they press the right bumper, use the right joystick for forward and
	// backward motion and the left joystick for turning
	else if(DriveController.GetBumper(GenericHID::JoystickHand::kRightHand))
	{
		forwardSpeed = DriveController.GetY(GenericHID::JoystickHand::kRightHand);
		turnSpeed = DriveController.GetX(GenericHID::JoystickHand::kLeftHand);
	}

	// Ensure the robot doesn't drive at full speed while the elevator is up
	if(IsElevatorTooHigh())
	{
		forwardSpeed *= consts::DRIVE_SPEED_REDUCTION;
		turnSpeed *= consts::DRIVE_SPEED_REDUCTION;
	}

	SmartDashboard::PutBoolean("Drive Speed Reduction?", IsElevatorTooHigh());

	// Negative is used to make forward positive and backwards negative
	// because the y-axes of the XboxController are natively inverted
	DriveTrain.ArcadeDrive(-forwardSpeed, turnSpeed);
}

// Operator controls. This function or the Elevator() function will not be run during
// the same teleopPeriodic() loop
void Robot::ManualElevator()
{
	// Use the right trigger to manually raise the elevator and
	// the left trigger to lower the elevator
	double raiseElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kRightHand));
	double lowerElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kLeftHand));

	double elevatorSpeed;

	bool overridesBeingPressed = OperatorController.GetStartButton();
//	bool rightBumperJustReleased = m_prevRBumperState && !OperatorController.GetBumper(GenericHID::kRightHand);
//	bool leftBumperJustReleased = m_prevLBumperState && !OperatorController.GetBumper(GenericHID::kLeftHand);
//	bool overridesJustReleased = ( (leftBumperJustReleased && !OperatorController.GetBumper(GenericHID::kRightHand)) ||
//			(rightBumperJustReleased && !OperatorController.GetBumper(GenericHID::kLeftHand)) ||
//			(rightBumperJustReleased && leftBumperJustReleased));
	bool overridesJustReleased = OperatorController.GetBackButton();
	SmartDashboard::PutBoolean("Zeroing Elevator Encoder?", overridesJustReleased);
//	OperatorController.GetBumperReleased()

	// If the two override keys are being pressed, allow the elevator to move past the predefined stop points
	// --> This is done to prevent a faulty start configuration from setting the lowest elevator setting at a higher point than
	//     intended
	if(overridesBeingPressed)
	{
		elevatorSpeed = dabs(raiseElevatorOutput) - dabs(lowerElevatorOutput);
	}
	// If the bumpers are released and the encoder position is negative, rezero the elevator at that point
	else if(overridesJustReleased)
	{
		elevatorSpeed = 0;
		RightElevatorMotor.SetSelectedSensorPosition(0, consts::PID_LOOP_ID, consts::TALON_TIMEOUT_MS);
	}
	else
	{
		elevatorSpeed = CapElevatorOutput(dabs(raiseElevatorOutput) - dabs(lowerElevatorOutput));
	}

	// If the triggers are being pressed
	if(raiseElevatorOutput != 0.0 || lowerElevatorOutput != 0.0)
	{
		ElevatorPIDController.Disable();
		RightElevatorMotor.Set(elevatorSpeed);
		LeftElevatorMotor.Set(elevatorSpeed);
		return;
	}
	else
	{
		RightElevatorMotor.Set(0);
		LeftElevatorMotor.Set(0);
	}

	// Test output
	SmartDashboard::PutBoolean("OverridesPressed", overridesBeingPressed);
	SmartDashboard::PutBoolean("OverridesReleased", overridesJustReleased);
	SmartDashboard::PutNumber("Elevator Height", ElevatorPID.PIDGet());

//	SmartDashboard::PutBoolean("Left Released", leftBumperJustReleased);
//	SmartDashboard::PutBoolean("Right Released", rightBumperJustReleased);
}

// Operator Controls
void Robot::Elevator()
{
	// Use the right trigger to manually raise the elevator and
	// the left trigger to lower the elevator
	double raiseElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kRightHand));
	double lowerElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kLeftHand));

	SmartDashboard::PutNumber("RaiseElev", raiseElevatorOutput);
	SmartDashboard::PutNumber("LowerElev", lowerElevatorOutput);

	// If either triggers are being pressed, disable the PID and
	// set the motor to the given speed
	if(raiseElevatorOutput != 0.0 || lowerElevatorOutput != 0.0)
	{
		ElevatorPIDController.Disable();
		double output = CapElevatorOutput(dabs(raiseElevatorOutput) - dabs(lowerElevatorOutput));
		RightElevatorMotor.Set(output);
		LeftElevatorMotor.Set(output);
		return;
	}
	else if(!ElevatorPIDController.IsEnabled())
	{
		RightElevatorMotor.Set(0);
		LeftElevatorMotor.Set(0);
	}

	// Automatic Mode is controlled by both bumpers
	if (OperatorController.GetBumperPressed(GenericHID::JoystickHand::kRightHand))
	{
		// If elevator is lowering and the right bumper is pressed, stop elevator where it is
		if (m_isElevatorLowering)
		{
			RightElevatorMotor.Set(0);
			LeftElevatorMotor.Set(0);
			m_isElevatorLowering = false;
			ElevatorPIDController.Disable();
		}
		else
		{
			// If right bumper is being pressed for the first time, increase the desired preset by 1
			if (!ElevatorPIDController.IsEnabled())
			{
				m_targetElevatorStep = GetClosestStepNumber();
			}
			// If right bumper has already been pressed, go to the next step.
			else if (m_targetElevatorStep < consts::NUM_ELEVATOR_SETPOINTS - 1)
			{
				m_targetElevatorStep++;
			}
			ElevatorPIDController.SetSetpoint(consts::ELEVATOR_SETPOINTS[m_targetElevatorStep]);
			ElevatorPIDController.Enable();
			m_isElevatorLowering = false;
		}
	}
	// The left bumper will lower the elevator to the bottom
	if (OperatorController.GetBumperPressed(GenericHID::JoystickHand::kLeftHand))
	{
		m_isElevatorLowering = true;
		ElevatorPIDController.SetSetpoint(consts::ELEVATOR_SETPOINTS[consts::ElevatorIncrement::GROUND]);
		ElevatorPIDController.Enable();
	}
}

void Robot::Intake()
{
	if(OperatorController.GetBumper(GenericHID::JoystickHand::kRightHand))
	{
		LeftSolenoid.Set(DoubleSolenoid::Value::kReverse);
		RightSolenoid.Set(DoubleSolenoid::Value::kReverse);
	}
	else if(OperatorController.GetBumper(GenericHID::JoystickHand::kLeftHand))
	{
		RightIntakeMotor.Set(-consts::INTAKE_SPEED);
		LeftIntakeMotor.Set(consts::INTAKE_SPEED);

//		if(!EjectTimer.m_running) EjectTimer.Start();
//
//		if(EjectTimer.Get() > consts::INTAKE_WAIT_TIME)
//		{
			LeftSolenoid.Set(DoubleSolenoid::Value::kForward);
			RightSolenoid.Set(DoubleSolenoid::Value::kForward);
//		}
	}
	else if(OperatorController.GetBButton())
	{
		RightIntakeMotor.Set(consts::INTAKE_SPEED);
		LeftIntakeMotor.Set(-consts::INTAKE_SPEED);
	}
	else if(OperatorController.GetAButton())
	{
		RightIntakeMotor.Set(-consts::INTAKE_SPEED);
		LeftIntakeMotor.Set(consts::INTAKE_SPEED);
	}		
	else
	{
		// Use the Right Y-axis for variable intake speed
		double intakeSpeed = applyDeadband(OperatorController.GetY(GenericHID::kRightHand));
		RightIntakeMotor.Set(intakeSpeed);
		LeftIntakeMotor.Set(-intakeSpeed);

		if(intakeSpeed == 0.0)
		{
//			if(LeftSolenoid.Get() == DoubleSolenoid::Value::kForward)
//				LeftIntakeMotor.Set(-consts::RESTING_INTAKE_SPEED);
//			if(RightSolenoid.Get() == DoubleSolenoid::Value::kForward)
//				RightIntakeMotor.Set(consts::RESTING_INTAKE_SPEED);
		}
	}

//	if(!OperatorController.GetBumper(GenericHID::JoystickHand::kRightHand))
//	{
//		EjectTimer.Stop();
//		EjectTimer.Reset();
//	}
}

void Robot::Linkage()
{
	// Use the left y-axis to do the linkage
	double motorSpeed = -OperatorController.GetY(GenericHID::JoystickHand::kLeftHand);
	LinkageMotor.Set(motorSpeed);
}

void Robot::TeleopInit()
{
	StopCurrentProcesses();
	LeftSolenoid.Set(DoubleSolenoid::Value::kForward);
	RightSolenoid.Set(DoubleSolenoid::Value::kForward);
}

void Robot::TeleopPeriodic()
{
	Drive();
	ManualElevator();
	Intake();
	Linkage();
}
