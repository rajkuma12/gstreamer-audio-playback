/*
Notes for compilation:
1. For compiling the code along with the Makefile given, a OE setup is mandatory. 
2. Before compiling, change the paths as per the setup of your environment.

Please refer the Gstreamer Application Development Manual at the below link before proceeding further
http://gstreamer.freedesktop.org/data/doc/gstreamer/head/manual/html/index.html

Comprehensive documentation for Gstreamer 
http://gstreamer.freedesktop.org/documentation/

The following elements/plugins/packages are expected to be in the module image for this to work
gstreamer
gst-plugins-base
gst-plugins-good-wavparse
gst-plugins-good-alsa
gst-plugins-good-audioconvert
gst-plugins-ugly-mad

Pipeline to play .wav audio file from command line
gst-launch filesrc location="location of file" ! wavparse ! alsasink 

Pipeline to play .mp3 audio file from command line
gst-launch filesrc location="location of file" ! mad ! audioconvert ! alsasink 

Please note that the above pipelines are meant to be used on Colibri Vybrid VF50/VF61 
It is also assumed that the USB to Audio device is the only audio device being used on the system, if not the 
"device" parameter for alsasink will change and the parameter to be used needs to be checked with cat /proc/asound/cards,
which then needs to be set as follows

In gstreamer pipeline 

Pipeline to play .wav audio file from command line
gst-launch filesrc location="location of file" ! wavparse ! alsasink device=hw:1,0

Pipeline to play .mp3 audio file from command line
gst-launch filesrc location="location of file" ! mad ! audioconvert ! alsasink device=hw:1,0

In code initialisation in init_audio_playback_pipeline
g_object_set (G_OBJECT (data->alsasink), "device", "hw:0,0", NULL);
							OR
g_object_set (G_OBJECT (data->alsasink), "device", "hw:1,0", NULL);

The pipeline will ideally remain the same for a different audio device, only the device parameter for alsasink will change

For gstreamer audio pipelines on other Colibri/Apalis modules refer the below link
http://developer.toradex.com/knowledge-base/audio-%28linux%29
*/

#include <gstreamer-0.10/gst/gst.h>
#include <gstreamer-0.10/gst/gstelement.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define NUMBER_OF_BYTES_FOR_FILE_LOCATION	256

volatile gboolean exit_flag = FALSE;

typedef struct  
{
	GstElement *file_source;
	GstElement *pipeline;
	GstElement *audio_decoder;	
	GstElement *audioconvert;
	GstElement *alsasink;	
	GstElement *bin_playback;	
	GstBus *bus;
	GstMessage *message;		
	gchar filelocation[NUMBER_OF_BYTES_FOR_FILE_LOCATION];
}gstData;

gstData gstreamerData;

// Create the pipeline element
gboolean create_pipeline(gstData *data)
{		
	data->pipeline = gst_pipeline_new("audio_pipeline");	
	if (data->pipeline == NULL)
	{			
		return FALSE;
	}
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	return TRUE;
}

// Callback function for dynamically linking the "wavparse" element and "alsasink" element
void on_pad_added (GstElement *src_element, GstPad *src_pad, gpointer data)
{
    g_print ("\nLinking dynamic pad between wavparse and alsasink\n");

    GstElement *sink_element = (GstElement *) data; 	// Is alsasink
    GstPad *sink_pad = gst_element_get_static_pad (sink_element, "sink");
    gst_pad_link (src_pad, sink_pad);

    gst_object_unref (sink_pad);
    src_element = NULL; 	// Prevent "unused" warning here
}

// Setup the pipeline
gboolean init_audio_playback_pipeline(gstData *data)
{
	if (data == NULL)
		return FALSE;
		
	data->file_source = gst_element_factory_make("filesrc", "filesource");	
	
	if (strstr(data->filelocation, ".mp3"))
	{
		g_print ("\nMP3 Audio decoder selected\n");
		data->audio_decoder = gst_element_factory_make("mad", "audiomp3decoder");
	}
	
	if (strstr(data->filelocation, ".wav"))
	{
		g_print ("\nWAV Audio decoder selected\n");
		data->audio_decoder = gst_element_factory_make("wavparse", "audiowavdecoder");
	}
		
	data->audioconvert = gst_element_factory_make("audioconvert", "audioconverter");	
	
	data->alsasink = gst_element_factory_make("alsasink", "audiosink");
	
	if ( !data->file_source || !data->audio_decoder || !data->audioconvert || !data->alsasink )
	{
		g_printerr ("\nNot all elements for audio pipeline were created\n");
		return FALSE;
	}	
	
	// Uncomment this if you want to see some debugging info
	//g_signal_connect( data->pipeline, "deep-notify", G_CALLBACK( gst_object_default_deep_notify ), NULL );	
	
	g_print("\nFile location: %s\n", data->filelocation);
	g_object_set (G_OBJECT (data->file_source), "location", data->filelocation, NULL);			
	
	data->bin_playback = gst_bin_new ("bin_playback");	
	
	if (strstr(data->filelocation, ".mp3"))
	{
		gst_bin_add_many(GST_BIN(data->bin_playback), data->file_source, data->audio_decoder, data->audioconvert, data->alsasink, NULL);
	
		if (gst_element_link_many (data->file_source, data->audio_decoder, NULL) != TRUE)
		{
			g_printerr("\nFile source and audio decoder element could not link\n");
			return FALSE;
		}
	
		if (gst_element_link_many (data->audio_decoder, data->audioconvert, NULL) != TRUE)
		{
			g_printerr("\nAudio decoder and audio converter element could not link\n");
			return FALSE;
		}
	
		if (gst_element_link_many (data->audioconvert, data->alsasink, NULL) != TRUE)
		{
			g_printerr("\nAudio converter and audio sink element could not link\n");
			return FALSE;
		}
	}
	
	if (strstr(data->filelocation, ".wav"))
	{
		gst_bin_add_many(GST_BIN(data->bin_playback), data->file_source, data->audio_decoder, data->alsasink, NULL);
	
		if (gst_element_link_many (data->file_source, data->audio_decoder, NULL) != TRUE)
		{
			g_printerr("\nFile source and audio decoder element could not link\n");
			return FALSE;
		}
	
		// Avoid checking of return value for linking of "wavparse" element and "alsasink" element
		// Refer http://stackoverflow.com/questions/3656051/unable-to-play-wav-file-using-gstreamer-apis
		
		gst_element_link_many (data->audio_decoder, data->alsasink, NULL);
		
		g_signal_connect(data->audio_decoder, "pad-added", G_CALLBACK(on_pad_added), data->alsasink);	
	}	
	
	return TRUE;
}

// Starts the pipeline 
gboolean start_playback_pipe(gstData *data)
{
	// http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer/html/GstElement.html#gst-element-set-state
	gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
	while(gst_element_get_state(data->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_SUCCESS);	
	return TRUE;
}

// Add the pipeline to the bin
gboolean add_bin_playback_to_pipe(gstData *data)
{
	if((gst_bin_add(GST_BIN (data->pipeline), data->bin_playback)) != TRUE)
	{
		g_print("\nbin_playback not added to pipeline\n");
		return FALSE;	
	}
	
	if(gst_element_set_state (data->pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS)
	{		
		return TRUE;
	}
	else
	{
		g_print("\nFailed to set pipeline state to NULL\n");
		return FALSE;		
	}
}

// Disconnect the pipeline and the bin
void remove_bin_playback_from_pipe(gstData *data)
{
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	gst_element_set_state (data->bin_playback, GST_STATE_NULL);
	if((gst_bin_remove(GST_BIN (data->pipeline), data->bin_playback)) != TRUE)
	{
		g_print("\nbin_playback not removed from pipeline\n");
	}	
}

// Cleanup
void delete_pipeline(gstData *data)
{
	if (data->pipeline)
		gst_element_set_state (data->pipeline, GST_STATE_NULL);	
	if (data->bus)
		gst_object_unref (data->bus);
	if (data->pipeline)
		gst_object_unref (data->pipeline);	
}

// Function for checking the specific message on bus
// We look for EOS or Error messages
gboolean check_bus_cb(gstData *data)
{
	GError *err = NULL;                
	gchar *dbg = NULL;   
		  
	g_print("\nGot message: %s\n", GST_MESSAGE_TYPE_NAME(data->message));
	switch(GST_MESSAGE_TYPE (data->message))
	{
		case GST_MESSAGE_EOS: 	  
			g_print ("\nEnd of stream... \n\n");
			exit_flag = TRUE;
			break;

		case GST_MESSAGE_ERROR: 
			gst_message_parse_error (data->message, &err, &dbg);
			if (err) 
			{
				g_printerr ("\nERROR: %s\n", err->message);
				g_error_free (err);
			}
			if (dbg) 
			{
				g_printerr ("\nDebug details: %s\n", dbg);
				g_free (dbg);
			}
			exit_flag = TRUE;
			break;

		default:
			g_printerr ("\nUnexpected message of type %d\n", GST_MESSAGE_TYPE (data->message));
			break;
	}
	return TRUE;
}

int main(int argc, char *argv[])
{	
	if (argc != 2)
	{
		g_print("\nUsage: ./audiovf /home/root/filename.mp3\n");
		g_print("Usage: ./audiovf /home/root/filename.wav\n");
		g_print("Note: Number of bytes for file location: %d\n\n", NUMBER_OF_BYTES_FOR_FILE_LOCATION);
		return FALSE;
	}
	
	if ((!strstr(argv[1], ".mp3")) && (!strstr(argv[1], ".wav")))
	{
		g_print("\nOnly mp3 & wav files can be played\n");
		g_print("Specify the mp3 or wav file to be played\n");
		g_print("Usage: ./audiovf /home/root/filename.mp3\n");
		g_print("Usage: ./audiovf /home/root/filename.wav\n");
		g_print("Note: Number of bytes for file location: %d\n\n", NUMBER_OF_BYTES_FOR_FILE_LOCATION);
		return FALSE;
	}	 
	
	// Initialise gstreamer. Mandatory first call before using any other gstreamer functionality
	gst_init (&argc, &argv); 
	
	memset(gstreamerData.filelocation, 0, sizeof(gstreamerData.filelocation));
	strcpy(gstreamerData.filelocation, argv[1]);		
	
	if (!create_pipeline(&gstreamerData))
		goto err;		
	
	if(init_audio_playback_pipeline(&gstreamerData))
	{	
		if(!add_bin_playback_to_pipe(&gstreamerData))
			goto err;		
		
		if(start_playback_pipe(&gstreamerData))
		{
			gstreamerData.bus = gst_element_get_bus (gstreamerData.pipeline);
			
			while (TRUE)
			{
				if (gstreamerData.bus)
				{	
					// Check for End Of Stream or error messages on bus
					// The global exit_flag will be set in case of EOS or error. Exit if the flag is set
					gstreamerData.message = gst_bus_poll (gstreamerData.bus, GST_MESSAGE_EOS | GST_MESSAGE_ERROR, -1);
					if(GST_MESSAGE_TYPE (gstreamerData.message))
					{
						check_bus_cb(&gstreamerData);
					}
					gst_message_unref (gstreamerData.message);			
				}			
				
				if (exit_flag)
					break;			
				
				sleep(1);				
			}					
		}	
		remove_bin_playback_from_pipe(&gstreamerData);					
	}	

err:	
	delete_pipeline(&gstreamerData);
	
	return TRUE;
}
