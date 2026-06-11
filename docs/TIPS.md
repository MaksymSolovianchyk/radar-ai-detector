## Important information
- If your board has strange SPI output or you have some issues and want to find the real problem, IN main branch in the radar_prj project -> main.c in the end of the file are test funtions by enabling which you can check SPI board for issues related to communication.
## TODO:
Add instructions or different branches where will be code to be able to use PY scripts. code to enable dataset-collector is in
while (1)
    {
    	if (FFT_IsReady())
		{
			FFT_Process();
			FFT_Transmit();
			while (!FFT_TransmitDone()) { __NOP(); }
			FFT_Reset();
		}

        if (commandflag)
        {
            __disable_irq();
            uint8_t local_cmd[3];
            local_cmd[0] = cmd_buffer[0];
            local_cmd[1] = cmd_buffer[1];
            local_cmd[2] = cmd_buffer[2];
            __enable_irq();
            commandflag = 0;
            CommandHandler(local_cmd);
        }
        /* USER CODE END WHILE */
    }

  this code must be also tested
