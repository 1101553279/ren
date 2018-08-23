static err_t http_recv(void *arg, struct tcp_pcb *pcb,  struct pbuf *p, err_t err)
{
  int32_t i,len=0;
  uint32_t DataOffset, FilenameOffset;
  char *data, *ptr, filename[13], login[LOGIN_SIZE]; 
  char message[20];
  struct fs_file file = {0, 0};
  struct http_state *hs;
  
  hs = arg;

  if (err == ERR_OK && p != NULL)
  {
    /* Inform TCP that we have taken the data */
    tcp_recved(pcb, p->tot_len);
    
    if (hs->file == NULL)
    {
      data = p->payload;
      len = p->tot_len;
      
      /* process HTTP GET requests*/
      if (strncmp(data, "GET /", 5) == 0)
      {
        if ((strncmp(data, "GET /resetmcu.cgi", 17) ==0)&&(htmlpage == UploadDonePage))
        {
          htmlpage = ResetDonePage;
          fs_open("/reset.html", &file);
          hs->file = file.data;
          hs->left = file.len;
          pbuf_free(p);
          
          /* send reset.html page */ 
          send_data(pcb, hs);   
          resetpage = 1;
          
          /* Tell TCP that we wish be to informed of data that has been
          successfully sent by a call to the http_sent() function. */
          tcp_sent(pcb, http_sent);
        }     
        else
        {
          /*send the login page (which is the index page) */
          htmlpage = LoginPage;
          fs_open("/index.html", &file);
          hs->file = file.data;
          hs->left = file.len;
          pbuf_free(p);
                  
          /* send index.html page */ 
          send_data(pcb, hs);
          
          /* Tell TCP that we wish be to informed of data that has been
          successfully sent by a call to the http_sent() function. */
          tcp_sent(pcb, http_sent);
        }
      }
      /* process POST request for checking login */
      else if ((strncmp(data, "POST /checklogin.cgi",20)==0)&&(htmlpage== LoginPage))
      {
          /* parse packet for the username & password */
          for (i=0;i<len;i++)
          {
             if (strncmp ((char*)(data+i), "username=", 9)==0)
             {            
               sprintf(login,"username=%s&password=%s&",USERID,PASSWORD);
               if (strncmp((char*)(data+i), login ,LOGIN_SIZE)==0)
               {
                 htmlpage = FileUploadPage;
                 fs_open("/upload.html", &file);
               }
               else
               {
                 htmlpage = LoginPage;
                 /* reload index.html */
                 fs_open("/index.html", &file);
               }
               
               hs->file = file.data;
               hs->left = file.len;
               
               pbuf_free(p);
               
               /* send index.html page */ 
               send_data(pcb, hs);
        
               /* Tell TCP that we wish be to informed of data that has been
               successfully sent by a call to the http_sent() function. */
               tcp_sent(pcb, http_sent); 
               break;
             }
          }     
      }
      /* process POST request for file upload and incoming data packets after POST request*/
      else if (((strncmp(data, "POST /upload.cgi",16)==0)||(DataFlag >=1))&&(htmlpage == FileUploadPage))
      { 
        DataOffset =0;
        
        /* POST Packet received */
        if (DataFlag ==0)
        { 
          BrowserFlag=0;
          TotalReceived =0;
          
          /* parse packet for Content-length field */
          size = Parse_Content_Length(data, p->tot_len);
           
          /* parse packet for the octet-stream field */
          for (i=0;i<len;i++)
          {
             if (strncmp ((char*)(data+i), octet_stream, 13)==0)
             {
               DataOffset = i+16;
               break;
             }
          }  
          /* case of MSIE8 : we do not receive data in the POST packet*/ 
          if (DataOffset==0)
          {
             DataFlag++;
             BrowserFlag = 1;
             pbuf_free(p);
             return ERR_OK;
             
          }
          /* case of Mozilla Firefox v3.6 : we receive data in the POST packet*/
          else
          {
            TotalReceived = len - (ContentLengthOffset + 4);
          }
        }
        
        if (((DataFlag ==1)&&(BrowserFlag==1)) || ((DataFlag ==0)&&(BrowserFlag==0)))
        { 
           if ((DataFlag ==0)&&(BrowserFlag==0)) 
           {
             DataFlag++;
           }
           else if ((DataFlag ==1)&&(BrowserFlag==1))
           {
             /* parse packet for the octet-stream field */
             for (i=0;i<len;i++)
             {
               if (strncmp ((char*)(data+i), octet_stream, 13)==0)
               {
                 DataOffset = i+16;
                 break;
               }
             }
             TotalReceived+=len;
             DataFlag++;
           }  
                
           /* parse packet for the filename field */
           FilenameOffset = 0;
           for (i=0;i<len;i++)
           {
             if (strncmp ((char*)(data+i), "filename=", 9)==0)
             {
                FilenameOffset = i+10;
                break;
             }
           }           
           i =0;
           if (FilenameOffset)
           {
             while((*(data+FilenameOffset + i)!=0x22 )&&(i<13))
             {
               filename[i] = *(data+FilenameOffset + i);
               i++;
             }
             filename[i] = 0x0;
           }
           
           if (i==0)
           {
             htmlpage = FileUploadPage;
             /* no filename, in this case reload upload page */
             fs_open("/upload.html", &file);
             hs->file = file.data;
             hs->left = file.len;
             pbuf_free(p);
             
             /* send index.html page */ 
             send_data(pcb, hs);
          
             /* Tell TCP that we wish be to informed of data that has been
             successfully sent by a call to the http_sent() function. */
             tcp_sent(pcb, http_sent); 
             DataFlag=0;
             return ERR_OK;
             
           }
             
          
           TotalData =0 ;
           /* init flash */
           FLASH_If_Init();
           /* erase user flash area */
           FLASH_If_Erase(USER_FLASH_FIRST_PAGE_ADDRESS);
          
           FlashWriteAddress = USER_FLASH_FIRST_PAGE_ADDRESS;
          
         }
         /* DataFlag >1 => the packet is data only  */
         else 
         {
           TotalReceived +=len;
         }
        
         ptr = (char*)(data + DataOffset);
         len-= DataOffset;
        
         /* update Total data received counter */
         TotalData +=len;
        
         /* check if last data packet */
         if (TotalReceived == size)
         {
           /* write data in Flash */
           if (len)
            IAP_HTTP_writedata(ptr,len);
          
           DataFlag=0;
          
          htmlpage = UploadDonePage;
          /* send uploaddone.html page */
          fs_open("/uploaddone.html", &file);
          hs->file = file.data;
          hs->left = file.len;
          send_data(pcb, hs);
          
          /* Tell TCP that we wish be to informed of data that has been
           successfully sent by a call to the http_sent() function. */
          tcp_sent(pcb, http_sent);  
        }
        /* not last data packet */
        else
        {
          /* write data in flash */
          if(len)
          IAP_HTTP_writedata(ptr,len);
        }
        pbuf_free(p);
      }
      else
      {
        pbuf_free(p);
        close_conn(pcb, hs);
      }
    }
    else
    {
      pbuf_free(p);
      close_conn(pcb,hs);
    }
  }
  
  if (err == ERR_OK && p == NULL)
  {
    close_conn(pcb, hs);
  }
  
  return ERR_OK;
}


