#include "Robot.h"

void Robot::TestInit()
{
	SmartDashboard::PutBoolean("Drive", 0);
	SmartDashboard::PutBoolean("Full Elevator", 0);
	SmartDashboard::PutBoolean("Manual Elevator", 0);
	SmartDashboard::PutBoolean("PID Elevator", 0);
	SmartDashboard::PutBoolean("Linkage", 0);
	SmartDashboard::PutBoolean("Intake", 0);
	SmartDashboard::PutBoolean("Climb", 0);
}

void Robot::TestPeriodic()
{
	if(SmartDashboard::GetBoolean("Drive", 0)) DriveTest();
	if(SmartDashboard::GetBoolean("Full Elevator", 0))
	{
		FullElevatorTest();
		SmartDashboard::PutBoolean("Full Elevator", 0);
		SmartDashboard::PutBoolean("Manual Elevator", 0);
	}
	if(SmartDashboard::GetBoolean("Manual Elevator", 0)) ManualElevatorTest();
	if(SmartDashboard::GetBoolean("PID Elevator", 0)) PIDElevatorTest();
	if(SmartDashboard::GetBoolean("Linkage", 0)) LinkageTest();
	if(SmartDashboard::GetBoolean("Intake", 0)) IntakeTest();
	if(SmartDashboard::GetBoolean("Climb", 0)) ClimbTest();
}


void Robot::RunMotorsTestFor(int numberOfSeconds)
{
	DriveTrain.ArcadeDrive(1, 0);
	Wait(numberOfSeconds / 2);
	DriveTrain.ArcadeDrive(-1, 0);
	Wait(numberOfSeconds / 2);
	DriveTrain.ArcadeDrive(0, 0);
}

void Robot::DriveTest()
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

	// Negative is used to make forward positive and backwards negative
	// because the y-axes of the XboxController are natively inverted
	DriveTrain.ArcadeDrive(-forwardSpeed, turnSpeed);
}

void Robot::ManualElevatorTest()
{
	// Use the right trigger to manually raise the elevator and
	// the left trigger to lower the elevator
	double raiseElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kRightHand));
	double lowerElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kLeftHand));

	SmartDashboard::PutNumber("RaiseElev", raiseElevatorOutput);
	SmartDashboard::PutNumber("LowerElev", lowerElevatorOutput);

	if(raiseElevatorOutput != 0.0 || lowerElevatorOutput != 0.0)
	{
		ElevatorPIDController.Disable();
		double output = CapElevatorOutput(dabs(raiseElevatorOutput) - dabs(lowerElevatorOutput));
		ElevatorMotor.Set(output);
		return;
	}
	else if(!ElevatorPIDController.IsEnabled())
	{
		ElevatorMotor.Set(0);
	}
}

void Robot::CapElevatorSetpoint(double& setpoint)
{
	// Prevent the setpoint from dipping below the min
	if(setpoint < consts::ELEVATOR_SETPOINTS[0])
	{
		setpoint = 5.0;
	}
	// Prevent the setpoint from exceeding the max
	else if(setpoint > consts::ELEVATOR_SETPOINTS[consts::NUM_ELEVATOR_SETPOINTS])
	{
		setpoint = 65.0;
	}
	else
	{
		return;
	}
}

void Robot::PIDElevatorTest()
{
	// Use the right trigger to manually raise the elevator and
	// the left trigger to lower the elevator
	double raiseElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kRightHand));
	double lowerElevatorOutput = applyDeadband(OperatorController.GetTriggerAxis(
			GenericHID::JoystickHand::kLeftHand));

	// If either triggers are being pressed
	if(raiseElevatorOutput != 0.0 || lowerElevatorOutput != 0.0)
	{
		// Output ranges from -1 to 1 and will act as a multiplier for the max increment
		double output = CapElevatorOutput(dabs(raiseElevatorOutput) - dabs(lowerElevatorOutput));

		// If the elevator is in automatic mode, turn it off, and set the desired height to
		// the current height plus some increment
		if(m_isElevatorInAutoMode)
		{
			m_isElevatorInAutoMode = false;
			double desiredSetpoint = ElevatorPID.GetHeightInches() + consts::ELEVATOR_INCREMENT_PER_CYCLE * output;
			CapElevatorSetpoint(desiredSetpoint);
			ElevatorPIDController.SetSetpoint(desiredSetpoint);
		}
		else // If automatic mode isn't on, just increment the previous setpoint
		{
			double desiredSetpoint = ElevatorPIDController.GetSetpoint() + consts::ELEVATOR_INCREMENT_PER_CYCLE * output;
			CapElevatorSetpoint(desiredSetpoint);
			ElevatorPIDController.SetSetpoint(desiredSetpoint);
		}
		ElevatorPIDController.Enable();
		return;
	}
	else // If neither of the triggers are being pressed, keep the elevator at its current height
	{
		ElevatorPIDController.SetSetpoint(ElevatorPID.GetHeightInches());
	}

	// Automatic Mode is controlled by both bumpers
	if (OperatorController.GetBumper(GenericHID::JoystickHand::kRightHand))
	{
		// If elevator is lowering and the right bumper is pressed, stop elevator where it is
		if (m_isElevatorLowering)
		{
			ElevatorPIDController.SetSetpoint(ElevatorPID.GetHeightInches());
			m_isElevatorLowering = false;
		}
		else
		{
			// If right bumper is being pressed for the first time, increase the desired preset by 1
			if (!m_isElevatorInAutoMode)
			{
				m_targetElevatorStep = GetClosestStepNumber();
			}
			// If right bumper has already been pressed, go to the next step.
			else if (m_targetElevatorStep < 4)
			{
				m_targetElevatorStep++;
			}
			ElevatorPIDController.SetSetpoint(consts::ELEVATOR_SETPOINTS[m_targetElevatorStep]);
			m_isElevatorInAutoMode = true;
			m_isElevatorLowering = false;
		}
	}
	// The left bumper will lower the elevator to the bottom
	else if (OperatorController.GetBumper(GenericHID::JoystickHand::kLeftHand))
	{
		m_isElevatorInAutoMode = true;
		m_isElevatorLowering = true;
		ElevatorPIDController.SetSetpoint(consts::ELEVATOR_SETPOINTS[0]);
	}

	ElevatorPIDController.Enable();

	SmartDashboard::PutBoolean("Lowering?", m_isElevatorLowering);
	SmartDashboard::PutBoolean("Automatic?", m_isElevatorLowering);
	SmartDashboard::PutNumber("Elevator Height", ElevatorPID.GetHeightInches());
	SmartDashboard::PutNumber("Elevator Setpoint", ElevatorPIDController.GetSetpoint());
	SmartDashboard::PutNumber("Elevator Output", ElevatorPIDController.Get());
}

void Robot::FullElevatorTest()
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
		ElevatorMotor.Set(output);
		return;
	}
	else if(!ElevatorPIDController.IsEnabled())
	{
		ElevatorMotor.Set(0);
	}

	// Automatic Mode is controlled by both bumpers
	if (OperatorController.GetBumper(GenericHID::JoystickHand::kRightHand))
	{
		// If elevator is lowering and the right bumper is pressed, stop elevator where it is
		if (m_isElevatorLowering)
		{
			ElevatorMotor.Set(0);
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
			else if (m_targetElevatorStep < 4)
			{
				m_targetElevatorStep++;
			}
			ElevatorPIDController.SetSetpoint(consts::ELEVATOR_SETPOINTS[m_targetElevatorStep]);
			ElevatorPIDController.Enable();
			m_isElevatorLowering = false;
		}
	}
	// The left bumper will lower the elevator to the bottom
	if (OperatorController.GetBumper(GenericHID::JoystickHand::kLeftHand))
	{
		m_isElevatorLowering = true;
		ElevatorPIDController.SetSetpoint(0);
		ElevatorPIDController.Enable();
	}
}

void Robot::LinkageTest()
{
	// Use the left y-axis to do the linkage
	LinkageMotor.Set(OperatorController.GetY(GenericHID::JoystickHand::kLeftHand));
}

void Robot::IntakeTest()
{
	// Use the B button to intake
	if(OperatorController.GetBButton() && IntakeUltrasonic.GetRangeInches() > consts::MIN_DISTANCE_TO_CUBE) // TODO change value
	{
		if((IntakeUltrasonic.IsEnabled() &&
					IntakeUltrasonic.GetRangeInches() > consts::MIN_DISTANCE_TO_CUBE) ||
					!IntakeUltrasonic.IsEnabled())
		{
			RightIntakeMotor.Set(consts::INTAKE_SPEED);
			LeftIntakeMotor.Set(-consts::INTAKE_SPEED);
		}
	}
	else
	{
		// Use the A button to eject if the B button is not being held
		if(OperatorController.GetAButton())
		{
			RightIntakeMotor.Set(-consts::INTAKE_SPEED);
			LeftIntakeMotor.Set(consts::INTAKE_SPEED);
		}
		else
		{
			RightIntakeMotor.Set(0);
			LeftIntakeMotor.Set(0);
		}
	}
}

void Robot::ClimbTest()
{
	// Use the y button to climb
	if(OperatorController.GetYButton())
	{
		ClimbMotor.Set(-1);
	}
	else
	{
		ClimbMotor.Set(0);
	}
}

void Robot::CurrentTest()
{
	SmartDashboard::PutNumber("FL Current",      FrontLeftMotor.GetOutputCurrent());
	SmartDashboard::PutNumber("FR Current",      FrontRightMotor.GetOutputCurrent());
	SmartDashboard::PutNumber("BL Current",      BackLeftMotor.GetOutputCurrent());
	SmartDashboard::PutNumber("BR Current",      BackRightMotor.GetOutputCurrent());
	SmartDashboard::PutNumber("RIntake Current", RightIntakeMotor.GetOutputCurrent());
	SmartDashboard::PutNumber("LIntake Current", LeftIntakeMotor.GetOutputCurrent());
}

