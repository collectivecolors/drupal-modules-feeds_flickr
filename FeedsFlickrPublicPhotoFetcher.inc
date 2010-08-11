<?php
// $Id$

/**
 * @file
 * Home of the FeedsFlickrPublicPhotoFetcher.
 */

/**
 * Fetches Flickr public photos based on sepecified users and/or tags.
 *   
 * This class basically exists to make creating feed nodes that fetch Flickr 
 * photos easier for the end user.
 */
class FeedsFlickrPublicPhotoFetcher extends FeedsHTTPFetcher {
  
  /**
   * Override parent::configDefaults().
   */
  public function configDefaults() {
    $defaults = parent::configDefaults();
    unset($defaults['auto_detect_feeds']);
    
    $defaults['photo_url'] = 'http://api.flickr.com/services/feeds/photos_public.gne';
    $defaults['format']    = 'atom_1';
    
    if (_feeds_flickr_include_phpflickr()) {
      $defaults['api_key'] = '';
    }    
    return $defaults;
  }

  /**
   * Override parent::configForm().
   */
  public function configForm(&$form_state) {
    $form = parent::configForm($form_state);
    unset($form['auto_detect_feeds']);
    
    $form['photo_url'] = array(
      '#type' => 'textfield',
      '#title' => t('Flickr feed endpoint'),
      '#description' => t('If you change this URL be sure that the same query parameters are used.  If you empty this input then submit the default URL will be used'),
      '#default_value' => $this->config['photo_url'],
    );    
    $form['format'] = array(
      '#type' => 'select',
      '#title' => t('Format'),
      '#options' => array(
        'atom_1' => t('Atom 1.0'),
        'rss_091' => t('RSS 0.91'),
        'rss_092' => t('RSS 0.92'),
        'rss_100' => t('RSS 1.0'),
        'rss_200' => t('RSS 2.0'),
        'rss_200_enc' => t('RSS 2.0 with enclosures (but without enclosure sizes)'),
      ),
      '#default_value' => $this->config['format'],
    );
    
    if (_feeds_flickr_include_phpflickr()) {
      $form['api_key'] = array(
        '#type' => 'textfield',
        '#title' => t('Optional Flickr API key'),
        '#description' => t('If a valid API key is entered then users creating feeds with this importer will be able to enter Flickr user names instead of their somewhat cryptic user ids.'),
        '#default_value' => $this->config['api_key'],
      );
    }
    
    return $form;
  }

  /**
   * Validation handler for configForm().
   *
   * Set errors with form_set_error().
   *
   * @param $values
   *   An array that contains the values entered by the user through configForm.
   */
  public function configFormValidate(&$values) {
    parent::configFormValidate($values);
    
    if (!$values['photo_url']) { // TODO: This should really be a check for a valid URL.
      $defaults = $this->configDefaults();
      $values['photo_url'] = $defaults['photo_url'];
    }
    
    if (_feeds_flickr_include_phpflickr() && $values['api_key']) {
      $flickr = new phpFlickr($values['api_key']);
      
      $flickr->test_echo();
      $error = $flickr->getErrorCode();
      
      if ($error && $error == 100) { // Check if we have a bad API key.
        form_set_error('api_key', t('You have specified an invalid API key.'));  
      }
    }
  }
  
  /**
   * Expose source form.
   */
  public function sourceForm($source_config) {
    
    // Check if we are using the Flickr APIs.
    if (_feeds_flickr_include_phpflickr() && $this->config['api_key']) {
      $flickr_api = TRUE;
      
      $form['use_api'] = array(
        '#type' => 'value',
        '#value' => $flickr_api,
      );
    }
    
    $labels = array(
      '%ids' => ($flickr_api ? t('names') : t('ids')),
    );
    
    $form['ids'] = array(
      '#type' => 'textfield',
      '#title' => t('Optional user %ids', $labels),
      '#description' => t('Enter one of more Flickr user %ids separated by a comma,.', $labels),
      '#default_value' => isset($source_config['ids']) ? $source_config['ids'] : '',
    );
    $form['tags'] = array(
      '#type' => 'textfield',
      '#title' => t('Optional photo tags'),
      '#description' => t('Enter one of more Flickr photo tags separated by a comma,.'),
      '#default_value' => isset($source_config['tags']) ? $source_config['tags'] : '',
    );
    $form['tag_mode'] = array(
      '#type' => 'select',
      '#title' => t('Tag mode'),
      '#options' => array(
        'ALL' => t('All tags required'),
        'ANY' => t('Any tag will work'),
      ),
      '#default_value' => isset($source_config['tag_mode']) ? $source_config['tag_mode'] : 'ALL',
    );
    $form['lang'] = array(
      '#type' => 'select',
      '#title' => t('Language'),
      '#options' => array(
        'en-us' => t('English'),
        'de-de' => t('German'),
        'es-us' => t('Spanish'),
        'fr-fr' => t('French'),
        'it-it' => t('Italian'),
        'ko-kr' => t('Korean'),
        'pt-br' => t('Portuguese (Brazilian)'),
        'zh-hk' => t('Traditional Chinese (Hong Kong)'),
      ),
      '#default_value' => isset($source_config['lang']) ? $source_config['lang'] : 'en-us',
    );
    return $form;
  }

  /**
   * Override parent::sourceFormValidate().
   */
  public function sourceFormValidate(&$values) {
    
    $params = array(
      'format'  => $this->config['format'],
      'lang'    => $values['lang']
    );
    
    if ($values['ids']) {
      $ids    = array();
      $flickr = ($values['use_api'] ? new phpFlickr($this->config['api_key']) : NULL);
      
      foreach (explode(',', $values['ids']) as $id) {
        $id = trim($id);
        
        if ($flickr) {
          $person = $flickr->people_findByUsername($id);
          $error  = $flickr->getErrorCode();
          
          if ($error) {
            form_set_error('ids', t('Flickr user %name lookup failed with error: [ %code ] %msg', array(
              '%name' => $id,
              '%code' => $error, 
              '%msg'  => $flickr->getErrorMsg()
            )));
            continue;  
          }
          else {
            $id = $person['id'];
          }
        }
        $ids[] = $id;
      }
    
      $params['ids'] = implode(',', $ids);
    }
    
    if ($values['tags']) {
      $tags = array();
      
      // Clean up specified tags before query injection.
      foreach (explode(',', $values['tags']) as $tag) {
        $tags[] = trim($tag);
      }
      
      $params['tags']    = implode(',', $tags);
      $params['tagmode'] = $values['tag_mode'];  
    }
    
    $query = drupal_query_string_encode($params);
    $values['source'] = $this->config['photo_url'] . "?$query";    
  }
}