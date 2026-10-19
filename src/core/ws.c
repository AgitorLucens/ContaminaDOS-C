#include <libwebsockets.h>

#include "ws.h"
#include "main.h" // For PORT and MAX_MSG_PAYLOAD definitions

// --- WebSocket Callback ---

int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                  void *user, void *in, size_t len)
{
    struct client_data *new_client = NULL;
    struct client_data *current_client = NULL;
    struct client_data **ptr_to_client_ptr = NULL; // For linked list removal

    switch (reason)
    {
    // --- Handle New Connection ---
    case LWS_CALLBACK_ESTABLISHED:
        lwsl_user("Connection established: %p\n", wsi);

        // Allocate memory for the new client node
        new_client = (struct client_data *)malloc(sizeof(struct client_data));
        if (!new_client)
        {
            lwsl_err("Failed to allocate memory for new client\n");
            return -1; // Close connection if allocation fails
        }
        new_client->wsi = wsi;

        // Add the new client to the beginning of the list
        new_client->next = client_list;
        client_list = new_client;
        break;

    // --- Handle Client Disconnection ---
    case LWS_CALLBACK_CLOSED:
        lwsl_user("Connection closed: %p\n", wsi);

        // Find and remove the client from the list
        ptr_to_client_ptr = &client_list;
        while (*ptr_to_client_ptr)
        {
            if ((*ptr_to_client_ptr)->wsi == wsi)
            {
                struct client_data *temp = *ptr_to_client_ptr;
                *ptr_to_client_ptr = temp->next; // Unlink the node
                free(temp);                      // Free the memory
                break;                           // Found and removed
            }
            // Move to the address of the next pointer in the list
            ptr_to_client_ptr = &((*ptr_to_client_ptr)->next);
        }
        break;

    // --- Handle Data Received from Client ---
    case LWS_CALLBACK_RECEIVE:
        lwsl_user("Received data from %p: length %d\n", wsi, (int)len);

        // NOTE: This is a simplified broadcast. For production:
        // 1. Handle fragmented messages (check lws_is_final_fragment, buffer parts).
        // 2. Consider potential buffer overflows if `in` is directly reused unsafely.
        // 3. Handle potential large number of clients more efficiently (e.g., queues).
        // 4. Handle write errors / backpressure more robustly than just logging.

        // Create a temporary buffer to potentially null-terminate (optional, safer)
        unsigned char msg_buffer[MAX_MSG_PAYLOAD + LWS_PRE]; // Add LWS_PRE padding
        // Ensure length doesn't exceed buffer (minus space for null terminator if desired)
        size_t safe_len = (len < MAX_MSG_PAYLOAD) ? len : MAX_MSG_PAYLOAD;
        memcpy(msg_buffer + LWS_PRE, in, safe_len);
        // msg_buffer[LWS_PRE + safe_len] = '\0'; // Optional null termination for logging

        lwsl_user("Broadcasting message: %.*s\n", (int)safe_len, msg_buffer + LWS_PRE);

        // Iterate through all connected clients
        current_client = client_list;
        while (current_client)
        {
            // Don't send the message back to the original sender
            if (current_client->wsi != wsi)
            {
                // Attempt to write the message to the other client
                int sent = lws_write(current_client->wsi, msg_buffer + LWS_PRE, safe_len, LWS_WRITE_TEXT);
                if (sent < (int)safe_len)
                {
                    lwsl_err("Error writing message to client %p (sent %d/%d)\n", current_client->wsi, sent, (int)safe_len);
                    // LWS will typically queue partial sends and trigger WRITEABLE later
                }
                else
                {
                    lwsl_user("Sent message to client %p\n", current_client->wsi);
                }
                // Optional: Request writable callback if you need confirmation or have more to send
                // lws_callback_on_writable(current_client->wsi);
            }
            current_client = current_client->next;
        }
        break;

        // --- Optional: Handle Writeability if Buffering/Queuing is Implemented ---
        // case LWS_CALLBACK_SERVER_WRITEABLE:
        // {
        //     // If you implemented an outgoing message queue for this client (wsi),
        //     // dequeue messages and send them using lws_write here.
        //     // Request another WRITEABLE callback if more data remains in the queue.
        //     break;
        // }

    default:
        // Ignore other callback reasons for this simple example
        break;
    }

    return 0; // Indicate success
}