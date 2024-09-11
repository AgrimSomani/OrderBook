Real Time Orderbook simulation, with supports adding, modifying and cancelling order across five types:
- Fill or Kill
- Fill and Kill
- Market
- Good For Day
- Good Till Cancel 

To use:
- Add your instructions in the Instructions file, following the format below:
  - A/M/C(ADD, MODIFY or CANCEL) B/S (BUY/SELL) GoodTillCancel/Market/GoodTillDay/KillOrFill/KillAndFill (Type of order) 109 (Price) 10 (Quantity) 10 (Order id)
- Add a result line at the end of file, representing what the state of the orderbook should look like at the end of all the orders being executed, following the format below:
  - R (RESULT) 1 (Total quantity of orders left in the orderbook) 0 (Total Bid Quantity) 1 (Total Ask Quantity)
- Compile the cpp files, and then execute the main function in main.cpp
