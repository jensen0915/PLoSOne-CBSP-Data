BEGIN {
Total_Delivery_Delay = 0;
Total_Packet = 0;
request = 0;
reply = 0;
mact = 0;
error = 0;
Total_Receive = 0;
Total_RTR_Drop = 0;
Total_IFQ_Drop = 0;
}
{
Type=$1;
Current_Time=$2;
SRC=$3;
DST=$4;
Network_Trace_Level=$4;
Packet_ID=$11;
Packet_Type=$5;
Packet_Type2=$17;
DSN=$18;
if(Type =="+" && Packet_Type =="tcp" && SRC == "3" && DST == "10" && Packet_Type2 == "D") {		
		print(Current_Time, "\tsubflow 1\t", DSN);
		Total_Packet++;
		}
if(Type =="+" && Packet_Type =="tcp" && SRC == "5" && DST == "12" && Packet_Type2 == "D") {		
		print(Current_Time, "\tsubflow 3\t", DSN);
		Total_Packet++;
		}
if(Type =="+" && Packet_Type =="tcp" && SRC == "4" && DST == "11" && Packet_Type2 == "D") {		
		print(Current_Time, "\tsubflow 2\t", DSN);
		Total_Packet++;
		}		
}