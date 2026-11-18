idf_component_register(SRCS "main.c"
                       EMBED_TXTFILES "index.html"
                       PRIV_REQUIRES esp_driver_gpio esp_event esp_http_server esp_netif esp_timer)
