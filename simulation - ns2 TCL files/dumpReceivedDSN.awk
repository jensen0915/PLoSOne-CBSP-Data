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
Size=$6;
Packet_ID=$11;
Packet_Type=$5;
Packet_Type2=$17;
DSN=$18;
if(Type =="r" && Packet_Type =="tcp" && Packet_Type2 =="D" && SRC == "18" && DST == "8") {		
		print(Current_Time, "\tsubflow 3\t", DSN);
		Total_Packet++;
		}
if(Type =="r" && Packet_Type =="tcp" && Packet_Type2 =="D" && SRC == "17" && DST == "7") {		
		print(Current_Time, "\tsubflow 2\t", DSN);
		Total_Packet++;
		}
if(Type =="r" && Packet_Type =="tcp" && Packet_Type2 =="D" && SRC == "16" && DST == "6") {		
		print(Current_Time, "\tsubflow 1\t", DSN);
		Total_Packet++;
		}		
}
