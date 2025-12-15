C/C++,Java,C#

Bank accounts

At a bank, we have to keep track of the balance of some accounts.

We have concurrently run transfer operations, to be executer on multiple threads. Each operation transfers a given amount of money from one account to someother account.

From time to time, as well as at the end of the program, a consistency check shall be executed. It shall verify that the total amount of money in all accounts is the same as in the beginning.

Two transaction involving distinct accounts must be able to proceed independently (without having to wait for the same mutex).

ex:
PR. I
Bank account:
- Id : int
- Balance: int


OPERATiON:
- CHUOSE RANDOM "FROM" ACCOUNT
- CHUOSE RANDOM "TO" ACCOUNT
- FROM!=TO
- LOCK ACCOUNT WITH ACCOUNT ID <
- LOCK ACCOUNT WITH ACCOUNT ID >
- TRANSFER MONEY
- UNLOCK iN REVERSE ORDER