class MainMenuClass {

  private:
    int scrollMainMenu = 0;

  public:
    bool active = false;
    int scrollMainMenuDir = 0;
    
    void activate();
    void handle();
  };

extern MainMenuClass MAINMENU;