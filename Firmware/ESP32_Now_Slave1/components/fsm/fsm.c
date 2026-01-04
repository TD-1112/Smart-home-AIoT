#include "fsm.h"

int16_t length_message = 0;
uint8_t flag_new_message;
uint16_t count_element_arr;
uint16_t data_after_length;

int32_t timeout_start;
int32_t timeout_wait;
fsmListState_e fsm_state;
uint8_t fsm_message_buffer[FSM_MAX_FRAME_SIZE];

static void ClearState(void);
static void Time_Out_Get_Message(void);
/**
   @brief : Flag of the new message

   @param lenght : Length of the new message
   @return uint16_t : Return 1 if received successfully message, else return 0
*/
uint16_t Is_Message(uint16_t *lenght)
{
  Time_Out_Get_Message();
  if (flag_new_message == TRUE)
  {
    timeout_wait = FALSE;
    flag_new_message = FALSE;
    *lenght = length_message;
    return 1;
  }
  else
  {
    return 0;
  }
}

/**
   @brief Get the Message:: Time Out object
   time to receive the new message
   used to avoid the case where the message is long or the message is shorter than the message length
*/
void Time_Out_Get_Message(void)
{
  if (timeout_wait == TRUE)
  {
    if (timeout_start >= COUNTER_TIMEOUT)
    {
      length_message = 0;
      ClearState();
    }
    timeout_start++;
  }
  else
  {
    ClearState();
  }
}

/**
   @brief Get the message from message buffer received from serial port
   @param datain : One byte data receive
   @param arr_message : Array data out of message
*/
void fsm_get_message(uint8_t datain, uint8_t arr_message[])
{
  // Keep the current frame intact until the application reads it.
  if (flag_new_message == TRUE)
  {
    return;
  }

  timeout_wait = TRUE;
  timeout_start = 0;

  if (count_element_arr >= FSM_MAX_FRAME_SIZE)
  {
    ClearState();
  }

  arr_message[count_element_arr] = datain;
  count_element_arr++;

  switch (fsm_state)
  {
  case FSM_STATE_START:
    if (count_element_arr == 1)
    {
      if (arr_message[0] != START_BYTE)
      {
        ClearState();
      }
    }
    else if (count_element_arr == FSM_STATE_CHANGE_VALUE_WAIT)
    {
      if (arr_message[1] != START_BYTE_FOLLOW)
      {
        ClearState();
      }
      else
      {
        fsm_state = FSM_STATE_WAIT;
      }
    }
    break;

  case FSM_STATE_WAIT:
    if (count_element_arr >= FRAME_HEADER_SIZE)
    {
      data_after_length = math.convert.bytes_to_uint16(arr_message[3], arr_message[4]);

      if (data_after_length < FRAME_MIN_LENGTH || data_after_length > FSM_MAX_FRAME_SIZE)
      {
        ClearState();
      }
      else
      {
        fsm_state = FSM_STATE_END;
      }
    }
    break;

  case FSM_STATE_END:
    if (count_element_arr == data_after_length)
    {
      flag_new_message = TRUE;
      length_message = count_element_arr;
      ClearState();
    }
    else if (count_element_arr > data_after_length)
    {
      ClearState();
    }
    break;
  }
}

/**
   @brief Used to reset elements when get message successfully or timeout.

*/
void ClearState(void)
{
  count_element_arr = 0;
  data_after_length = 0;
  timeout_start = 0;
  timeout_wait = FALSE;
  fsm_state = FSM_STATE_START;
}
