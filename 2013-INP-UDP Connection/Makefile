bsize = 200
tsize = 100
buffer = /tmp/buffer
input = /tmp/a_big_file
output = /tmp/output_file
client = /tmp/hw2/client
server = /tmp/hw2/server
sim_path = /home/estinet/9817024
sim_name = 9817024
buffer:
	-dd if=/dev/urandom of=$(buffer) bs=1M count=$(bsize)
create:
	-rm -f $(input)
	-dd if=$(buffer) of=$(input) bs=1M count=$(tsize)
alarm:
	@make clean
	@make create
	-g++ 1_client.cpp -o $(client)
	-g++ 1_server.cpp -o $(server)
select:
	@make clean
	@make create
	-g++ 2_client.cpp -o $(client)
	-g++ 2_server.cpp -o $(server)
setsockopt:
	@make clean
	@make create
	-g++ 3_client.cpp -o $(client)
	-g++ 3_server.cpp -o $(server)
competition:
	@make clean
	@make create
	-g++ competition_client.cpp -o $(client)
	-g++ competition_server.cpp -o $(server)
competition_SR:
	@make clean
	@make create
	-g++ competition_client_SR.cpp -o $(client)
	-g++ competition_server_SR.cpp -o $(server)
thrput:
	awk '{ sum += $$2 } END { print sum }' $(sim_path)/$(sim_name).results/$(sim_name).8023_N1_P1_OutThrput.log $(sim_path)/$(sim_name).results/$(sim_name).8023_N2_P1_OutThrput.log
diff:
	@-ls -al $(input) $(output)	
	@-md5sum $(input) $(output)
check:
	@echo "Throughout: "	
	@awk '{ sum += $$2 } END { print sum }' $(sim_path)/$(sim_name).results/$(sim_name).8023_N1_P1_OutThrput.log $(sim_path)/$(sim_name).results/$(sim_name).8023_N2_P1_OutThrput.log
	@-ls -al $(input) $(output)	
	@-md5sum $(input) $(output)
clean:
	-rm -f $(client) $(server) $(input) $(output)

