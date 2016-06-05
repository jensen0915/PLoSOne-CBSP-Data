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
if(Type =="d" && Packet_Type =="tcp" && SRC == "15" && DST == "18") {		
		print(Current_Time, "\t", Packet_ID);
		Total_Packet++;
		}
}