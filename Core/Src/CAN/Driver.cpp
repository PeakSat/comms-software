#include "CAN/Driver.hpp"

using namespace CAN;

extern FDCAN_HandleTypeDef hfdcan1;

void CAN::configCANFilter(){
    FDCAN_FilterTypeDef sFilterConfig;

    sFilterConfig.IdType        = FDCAN_STANDARD_ID;          // Standard or extended id
    sFilterConfig.FilterIndex   = 0;                          // In case of configuring multiple filters adapt accordingly
    sFilterConfig.FilterType    = FDCAN_FILTER_MASK;         // Filter type
    sFilterConfig.FilterConfig  = FDCAN_FILTER_TO_RXFIFO0;    // Where the messages that pass from the filter will go
    sFilterConfig.FilterID1     = 0x11; // 0x380;
    sFilterConfig.FilterID2     = 0x11; // 0x3FF;
    sFilterConfig.RxBufferIndex = 0;
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
    {
        /* Filter configuration Error */
        Error_Handler();
    }
}

void CAN::initialize() {
    configCANFilter();

    if(HAL_FDCAN_Start(&hfdcan1)!= HAL_OK){
        Error_Handler();
    }

    // Activate the notification for new data in FIFO0 for FDCAN1
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK){
        Error_Handler();
    }

    // Activate the notification for new data in FIFO1 for FDCAN1
//    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0) != HAL_OK)
//    {
//        /* Notification Error */
//        Error_Handler();
//    }

}

//void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs){
//    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
//    {
//        /* Retreive Rx messages from RX FIFO0 */
//        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader0, rxFifo0.data()) != HAL_OK)
//        {
//            /* Reception Error */
//            Error_Handler();
//        }
//
//        // logMessage(rxFifo0, rxHeader0, ActiveBus::Main);
//
//
//        if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
//        {
//            /* Notification Error */
//            Error_Handler();
//        }
//    }
//}

void CAN::logMessage(const CAN::CANBuffer_t &rxBuf, FDCAN_RxHeaderTypeDef RxHeader, CAN::ActiveBus incomingBus) {
    auto message = String<ECSSMaxStringSize>("CAN Message: ");
    if (incomingBus == Main) {
        message.append("FDCAN1 ");
    } else {
        message.append("FDCAN2 ");
    }
    uint32_t id = readId(RxHeader.Identifier);
    const uint8_t msgLength = RxHeader.DataLength;
    message.append("ID : ");
    etl::to_string(id, message, etl::format_spec().hex(), true);
    message.append(" Length : ");
    etl::to_string(msgLength, message, true);
    message.append(" Data : ");
    for (uint8_t idx = 0; idx < msgLength; idx++) {
        etl::to_string(*(rxBuf.data()+idx), message, true);
        message.append(" ");
    }
    LOG_INFO << message.c_str();

}

uint8_t CAN::convertDlcToLength(uint8_t dlc) {
    static constexpr uint8_t msgLength[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};
    return msgLength[dlc];
}

uint8_t CAN::convertLengthToDLC(uint8_t length){
    uint8_t dlc;

    if (length <= 8U) {
        dlc = length;
    } else if (length <= 12U) {
        dlc = 0x9U;
    } else if (length <= 16U) {
        dlc = 0xAU;
    } else if (length <= 20U) {
        dlc = 0xBU;
    } else if (length <= 24U) {
        dlc = 0xCU;
    } else if (length <= 32U) {
        dlc = 0xDU;
    } else if (length <= 48U) {
        dlc = 0xEU;
    } else {
        dlc = 0xFU;
    }
    return dlc;
}

void CAN::send(const CAN::Frame &message) {
    memset(CAN::txFifo.data(), 0, CANMessageSize);

    std::copy(message.data.begin(), message.data.end(), txFifo.data());

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &CAN::txHeader, txFifo.data())!= HAL_OK)
    {
        Error_Handler();
    }

}

Frame CAN::getFrame(const CAN::CANBuffer_t *TxFifo) {
    CAN::txHeader.Identifier            = CANIdentifier;
    CAN::txHeader.IdType                = FDCAN_STANDARD_ID;
    CAN::txHeader.TxFrameType           = FDCAN_DATA_FRAME;
    CAN::txHeader.DataLength            = FDCAN_DLC_BYTES_64;
    CAN::txHeader.ErrorStateIndicator   = FDCAN_ESI_ACTIVE;
    CAN::txHeader.BitRateSwitch         = FDCAN_BRS_OFF;
    CAN::txHeader.FDFormat              = FDCAN_FD_CAN;
    CAN::txHeader.TxEventFifoControl    = FDCAN_NO_TX_EVENTS;
    CAN::txHeader.MessageMarker         = 0;

    CAN::Frame frame;

    frame.id = readId(CAN::txHeader.Identifier);

    for (uint8_t idx = 0; idx < TxFifo->size(); idx++) {
        frame.data.insert_at(idx, *(TxFifo->data()+idx));
    }

    return frame;

}