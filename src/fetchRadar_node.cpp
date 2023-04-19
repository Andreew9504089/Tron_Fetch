#include <iostream>
#include <map>
#include <stdlib.h>
#include <typeinfo>
#include <string>

#include "ros/ros.h"
#include "std_msgs/String.h"
#include <tron_future/targets_arr.h>
#include <tron_future/target_gps.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#include "RSJparser.tcc"

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>
#include <amqpcpp/libev.h>

ros::Publisher radar_pub, vis_pub;

int main(int argc, char **argv) {
    ros::init(argc, argv, "fetchRadar");
    ros::NodeHandle nh;
    ros::Publisher radar_pub = nh.advertise<tron_future::targets_arr>("/target_gps", 1000);
    ros::Publisher vis_pub = nh.advertise<visualization_msgs::MarkerArray>( "/visualization_marker_array", 0 );
    std::map<std::string, std::vector<float>> tracklets_enu;
    std::map<std::string, std::vector<float>> tracklets_gps;

    // access to the event loop
    auto *loop = EV_DEFAULT;
    // handler for libev (so we don't have to implement AMQP::TcpHandler!)
    AMQP::LibEvHandler handler(loop);
    // address of the server
    AMQP::Address address("amqp://guest:guest@api-test.pub.tft.tw/");
    // create a AMQP connection object
    AMQP::TcpConnection connection(&handler, address);
    // and create a channel
    AMQP::TcpChannel channel(&connection);
    // use the channel object to call the AMQP method you like
    channel.declareQueue("");
    channel.bindQueue("api-event", "", "t_meta.surveillance.task.*.targets.updated");
    // callback function that is called when the consume operation starts
    auto startCb = [](const std::string &consumertag) {
        std::cout << "consume operation started" << std::endl;
    };
    // callback function that is called when the consume operation failed
    auto errorCb = [](const char *message) {
        std::cout << "consume operation failed" << std::endl;
    };

    // callback operation when a message was received
    auto messageCb = [&channel, radar_pub, vis_pub, &tracklets_enu, &tracklets_gps](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
        std::string s = message.body();
        std::string event_id{};
        std::string track_id{};
        std::vector<float> gps{0, 0, 0};
        std::vector<float> state{0, 0, 0, 0, 0, 0, 0, 0, 0}; // position

        //std::cout << "message received:" << s.substr(0, message.bodySize()) << "\n" << std::endl;
        // acknowledge the message
        channel.ack(deliveryTag);

        if(ros::ok()){
            std_msgs::String msg;
            float time;
            int target_type;
            tron_future::targets_arr detection_arr;
            tron_future::target_gps detection;
            RSJresource radar_raw (s.substr(0, message.bodySize()));

            event_id = radar_raw["event_id"].as<std::string>();
            detection_arr.event_id = event_id;
            for (int i = 0; i < radar_raw["data"]["targets_updated"].size(); i++){

                if(radar_raw["data"]["targets_updated"][i]["state_time_pairs"][0].size() > 0){

                    gps[0] = std::stof(radar_raw["data"]["targets_updated"][i]
                                            ["state_time_pairs"][0]
                                                ["position"]["lat"].as<std::string>());
                    gps[1] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                ["state_time_pairs"][0]
                                                    ["position"]["lng"].as<std::string>());
                    gps[2] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                ["state_time_pairs"][0]
                                                    ["position"]["alt"].as<std::string>());

                    track_id = radar_raw["data"]["targets_updated"][i]
                                                ["track_id"].as<std::string>();

                    state[0] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                    ["state_time_pairs"][0]
                                                         ["position_center"]["east"].as<std::string>());
                    state[1] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                    ["state_time_pairs"][0]
                                                        ["position_center"]["north"].as<std::string>());
                    state[2] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                    ["state_time_pairs"][0]
                                                        ["position_center"]["up"].as<std::string>());
                    state[0] = 0;std::stof(radar_raw["data"]["targets_updated"][i]
                                                     ["state_time_pairs"][0]
                                                        ["velocity"]["east"].as<std::string>());
                    state[1] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                   ["state_time_pairs"][0]
                                                        ["velocity"]["north"].as<std::string>());
                    state[2] = std::stof(radar_raw["data"]["targets_updated"][i]
                                                    ["state_time_pairs"][0]
                                                        ["velocity"]["up"].as<std::string>());

                    time = std::stof(radar_raw["data"]["targets_updated"][i]
                                                    ["state_time_pairs"][0]
                                                        ["timestamp"].as<std::string>());

                    target_type = std::stof(radar_raw["data"]["targets_updated"][i]
                                                    ["target_type"].as<std::string>());

                    detection.track_id = track_id;
                    detection.position_gps.x = gps[0];
                    detection.position_gps.y = gps[1];
                    detection.position_gps.z = gps[2];
                    detection.position_enu.x = state[0];
                    detection.position_enu.y = state[1];
                    detection.position_enu.z = state[2];
                    detection.linear.x = state[0];
                    detection.linear.y = state[1];
                    detection.linear.z = state[2];
                    detection.radar_stamp = time;
                    detection.local_stamp = ros::Time::now();
                    detection.target_type = target_type;
                    detection_arr.data.push_back(detection);

                    std::map<std::string,std::vector<float>>::iterator it = tracklets_enu.find(track_id);
                    std::map<std::string,std::vector<float>>::iterator it2 = tracklets_gps.find(track_id);
                    if(it == tracklets_enu.end()){
                        // track id does not exist
                        for(int i=0;i<3;i++){
                            state[6+i]=double(rand()%256)/256.;
                        }
                        tracklets_gps.insert(std::pair<std::string, std::vector<float>>(track_id, gps));
                        tracklets_enu.insert(std::pair<std::string, std::vector<float>>(track_id, state));
                    }else{
                        // track id exists
                        state[6] = it->second[6];
                        state[7] = it->second[7];
                        state[8] = it->second[8];
                        it->second = state;
                        it2->second = gps;
                    }
                }else{
                    continue;
                } 
            }

            visualization_msgs::MarkerArray marker_array;
            visualization_msgs::Marker marker, arrow, text;
            std::string id, gps_text;

            for(std::map<std::string,std::vector<float>>::iterator it = tracklets_enu.begin(); it != tracklets_enu.end(); ++it) {
                id = it->first;
                marker.header.frame_id = "map";
                marker.header.stamp = ros::Time();
                marker.ns = id;
                marker.id = 0;
                marker.type = visualization_msgs::Marker::SPHERE;
                marker.action = visualization_msgs::Marker::ADD;
                marker.pose.position.x = it->second[0];
                marker.pose.position.y = it->second[1];
                marker.pose.position.z = it->second[2];
                marker.pose.orientation.x = 0.0;
                marker.pose.orientation.y = 0.0;
                marker.pose.orientation.z = 0.0;
                marker.pose.orientation.w = 1.0;
                marker.scale.x = 30;
                marker.scale.y = 30;
                marker.scale.z = 30;
                marker.color.a = 1.0; // Don't forget to set the alpha!
                marker.color.r = it->second[6];
                marker.color.g = it->second[7];
                marker.color.b = it->second[8];
                marker_array.markers.push_back(marker);

                geometry_msgs::Point start, end;
                arrow.points.clear();
                start.x = it->second[0];
                start.y = it->second[1];
                start.z = it->second[2];
                end.x = it->second[0] + it->second[3]*5;
                end.y = it->second[1] + it->second[4]*5;
                end.z = it->second[2] + it->second[5]*5;
                arrow.points.push_back(start);
                arrow.points.push_back(end);
                arrow.header.frame_id = "map";
                arrow.header.stamp = ros::Time();
                arrow.ns = id;
                arrow.id = 1;
                arrow.type = visualization_msgs::Marker::ARROW;
                arrow.action = visualization_msgs::Marker::ADD;
                arrow.scale.x = 5;
                arrow.scale.y = 5;
                arrow.scale.z = 10;
                arrow.color.a = 1.0; // Don't forget to set the alpha!
                arrow.color.r = it->second[6];
                arrow.color.g = it->second[7];
                arrow.color.b = it->second[8];
                marker_array.markers.push_back(arrow);

                std::map<std::string,std::vector<float>>::iterator it2 = tracklets_gps.find(id);
                
                gps_text = "\n(" + std::to_string(it2->second[0])
                         + ",  " + std::to_string(it2->second[1])
                            + ", " + std::to_string(it2->second[2]) + ")";
                text.header.frame_id = "map";
                text.header.stamp = ros::Time();
                text.ns = id;
                text.id = 2;
                text.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
                text.action = visualization_msgs::Marker::ADD;
                text.pose.position.x = it->second[0];
                text.pose.position.y = it->second[1];
                text.pose.position.z = it->second[2] + 25;
                text.text = id + gps_text;
                text.scale.z = 15;
                text.color.a = 1.0; // Don't forget to set the alpha!
                text.color.r = 1.;
                text.color.g = 1.;
                text.color.b = 1.;
                marker_array.markers.push_back(text);
            }
            vis_pub.publish(marker_array);
            radar_pub.publish(detection_arr);

            marker_array.markers.clear();

            ros::spinOnce();
        }
    };
    // callback that is called when the consumer is cancelled by RabbitMQ (this only happens in
    // rare situations, for example when someone removes the queue that you are consuming from)
    auto cancelledCb = [](const std::string &consumertag) {
        std::cout << "consume operation cancelled by the RabbitMQ server" << std::endl;
    };
    // start consuming from the queue, and install the callbacks


    channel.consume("")
            .onReceived(messageCb)
            .onSuccess(startCb)
            .onCancelled(cancelledCb)
            .onError(errorCb);

    ev_run(loop, 0);
}
