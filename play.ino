

void play ( void *param ){
uint8_t playBuffer[32];

 Serial.printf("Playtask running on core %d\n", xPortGetCoreID()); 
//
//https://github.com/espressif/arduino-esp32/issues/595
//
  
  TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE;
  TIMERG0.wdt_feed=1;
  TIMERG0.wdt_wprotect=0;
 
  player.startSong();

  while ( uxQueueMessagesWaiting(playQueue) <  (PLAYQUEUESIZE/2) ) delay(5);
  
    while(1){

      xQueueReceive(playQueue, &playBuffer[0], portMAX_DELAY);

      if ( strncmp( (char *) &playBuffer[0], "ChangeStationSoStartANewSongNow!",32) == 0 ){
        player.stopSong();
        delay(5);
        player.startSong();
        xQueueReceive(playQueue, &playBuffer[0], portMAX_DELAY);
      }
      
      
        for ( int i = 0; i < 1 ; ++i ){
          if ( digitalRead( VS1053_DREQ ) ){
            xSemaphoreTake( tftSemaphore, portMAX_DELAY);
            player.playChunk(playBuffer, 32  );
            xSemaphoreGive( tftSemaphore);            
          }else{
            --i;           
            //Serial.printf ( "Waiting for VS1053, %d messages in playQueue\n", uxQueueMessagesWaiting( playQueue ) );
            delay(1);
          }
      }
     
    }


}


    /*--------------------------------------------------*/

int play_init(){
     
    xTaskCreatePinnedToCore( 
         play,                                      // Task to handle special functions.
         "Player",                                            // name of task.
         2048,                                                 // Stack size of task
         NULL,                                                 // parameter of the task
         PLAYTASKPRIO,                                                    // priority of the task
         &playTask,                                          // Task handle to keep track of created task 
         PLAYCORE);                                                  // processor core
                
return(0);
}
